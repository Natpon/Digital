library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity Elevator_Top is
    Port (
        clk        : in  STD_LOGIC;
        rst        : in  STD_LOGIC;
        sw_floor1  : in  STD_LOGIC;
        sw_floor2  : in  STD_LOGIC;
        sw_floor3  : in  STD_LOGIC;
        door_hold  : in  STD_LOGIC;
        pulse_in   : in  STD_LOGIC;
        rx_in      : in  STD_LOGIC;
        ena_pwm    : out STD_LOGIC;
        in1_dir    : out STD_LOGIC;
        in2_dir    : out STD_LOGIC;
        door_led   : out STD_LOGIC_VECTOR(4 downto 0);
        door_beep  : out STD_LOGIC;
        seg_out    : out STD_LOGIC_VECTOR(6 downto 0);
        seg_common : out STD_LOGIC_VECTOR(3 downto 0);
        tx_out     : out STD_LOGIC
    );
end Elevator_Top;

architecture Structural of Elevator_Top is
    signal rx_data        : STD_LOGIC_VECTOR(7 downto 0);
    signal rx_ready       : STD_LOGIC;
    signal tx_data        : STD_LOGIC_VECTOR(7 downto 0);
    signal tx_start       : STD_LOGIC := '0';
    signal tx_busy        : STD_LOGIC;

    signal sig_current_pos   : STD_LOGIC_VECTOR(15 downto 0);
    signal sig_target_pos    : STD_LOGIC_VECTOR(15 downto 0) := (others => '0');
    signal sig_motor_run     : STD_LOGIC;
    signal sig_motor_dir     : STD_LOGIC;
    signal sig_current_floor : STD_LOGIC_VECTOR(1 downto 0) := "01";
    signal last_floor        : STD_LOGIC_VECTOR(1 downto 0) := "01";

    constant DEBOUNCE_MAX : integer := 400000;
    signal db1_cnt, db2_cnt, db3_cnt : integer range 0 to DEBOUNCE_MAX := 0;
    signal sw1_db, sw2_db, sw3_db   : STD_LOGIC := '0';

    signal prev_sw1, prev_sw2, prev_sw3 : STD_LOGIC := '0';
    signal req_f1, req_f2, req_f3       : STD_LOGIC := '0';
    signal last_dir                     : STD_LOGIC := '1';

    constant POS_F1 : STD_LOGIC_VECTOR(15 downto 0) := std_logic_vector(to_unsigned(0,     16));
    constant POS_F2 : STD_LOGIC_VECTOR(15 downto 0) := std_logic_vector(to_unsigned(5000,  16));
    constant POS_F3 : STD_LOGIC_VECTOR(15 downto 0) := std_logic_vector(to_unsigned(10000, 16));

begin
    Inst_UART_RX: entity work.UART_RX
        generic map(CLKS_PER_BIT => 2083)
        port map(clk => clk, rst => rst, rx_in => rx_in,
                 data_out => rx_data, data_ready => rx_ready);

    Inst_UART_TX: entity work.UART_TX
        generic map(CLKS_PER_BIT => 2083)
        port map(clk => clk, rst => rst, tx_start => tx_start,
                 data_in => tx_data, tx_out => tx_out, tx_busy => tx_busy);

    Inst_Tracker: entity work.Floor_Position_Tracker
        port map(clk => clk, rst => rst, pulse_in => pulse_in,
                 dir_up => sig_motor_dir, current_pos => sig_current_pos);

    Inst_FSM: entity work.Elevator_FSM
        port map(clk => clk, rst => rst, target_floor => sig_target_pos,
                 current_pos => sig_current_pos, door_hold => door_hold,
                 motor_run => sig_motor_run, motor_dir => sig_motor_dir,
                 seg_common => seg_common, door_led => door_led, door_beep => door_beep);

    -- Debounce Filter (20ms @ 20MHz = 400000 cycles)
    process(clk, rst)
    begin
        if rst = '1' then
            db1_cnt <= 0; db2_cnt <= 0; db3_cnt <= 0;
            sw1_db  <= '0'; sw2_db <= '0'; sw3_db <= '0';
        elsif rising_edge(clk) then
            if sw_floor1 = sw1_db then
                db1_cnt <= 0;
            elsif db1_cnt = DEBOUNCE_MAX then
                db1_cnt <= 0; sw1_db <= sw_floor1;
            else
                db1_cnt <= db1_cnt + 1;
            end if;

            if sw_floor2 = sw2_db then
                db2_cnt <= 0;
            elsif db2_cnt = DEBOUNCE_MAX then
                db2_cnt <= 0; sw2_db <= sw_floor2;
            else
                db2_cnt <= db2_cnt + 1;
            end if;

            if sw_floor3 = sw3_db then
                db3_cnt <= 0;
            elsif db3_cnt = DEBOUNCE_MAX then
                db3_cnt <= 0; sw3_db <= sw_floor3;
            else
                db3_cnt <= db3_cnt + 1;
            end if;
        end if;
    end process;

    -- Request Flag (Rising Edge เท่านั้น + ใช้ sw_db แทน sw_floor)
    process(clk, rst)
    begin
        if rst = '1' then
            req_f1   <= '0'; req_f2   <= '0'; req_f3   <= '0';
            prev_sw1 <= '0'; prev_sw2 <= '0'; prev_sw3 <= '0';
        elsif rising_edge(clk) then
            if (sw1_db = '1' and prev_sw1 = '0') or (rx_ready = '1' and rx_data = x"31") then req_f1 <= '1'; end if;
            if (sw2_db = '1' and prev_sw2 = '0') or (rx_ready = '1' and rx_data = x"32") then req_f2 <= '1'; end if;
            if (sw3_db = '1' and prev_sw3 = '0') or (rx_ready = '1' and rx_data = x"33") then req_f3 <= '1'; end if;
            if sig_motor_run = '0' then
                if    sig_current_floor = "01" then req_f1 <= '0';
                elsif sig_current_floor = "10" then req_f2 <= '0';
                elsif sig_current_floor = "11" then req_f3 <= '0';
                end if;
            end if;
            prev_sw1 <= sw1_db; prev_sw2 <= sw2_db; prev_sw3 <= sw3_db;
        end if;
    end process;

    -- Queue Manager (เพิ่ม rst ให้ sig_target_pos และ last_dir)
    process(clk, rst)
        variable pos : integer;
    begin
        if rst = '1' then
            sig_target_pos <= (others => '0');
            last_dir       <= '1';
        elsif rising_edge(clk) then
            pos := to_integer(unsigned(sig_current_pos));
            if sig_motor_run = '1' then last_dir <= sig_motor_dir; end if;
            if sig_motor_run = '1' and sig_motor_dir = '1' then
                if req_f2 = '1' and pos < 4500 then
                    sig_target_pos <= POS_F2;
                elsif req_f3 = '1' then
                    sig_target_pos <= POS_F3;
                end if;
            elsif sig_motor_run = '1' and sig_motor_dir = '0' then
                if req_f2 = '1' and pos > 5500 then
                    sig_target_pos <= POS_F2;
                elsif req_f1 = '1' then
                    sig_target_pos <= POS_F1;
                end if;
            elsif sig_motor_run = '0' then
                if sig_current_floor = "01" then
                    if    req_f2 = '1' then sig_target_pos <= POS_F2;
                    elsif req_f3 = '1' then sig_target_pos <= POS_F3;
                    end if;
                elsif sig_current_floor = "11" then
                    if    req_f2 = '1' then sig_target_pos <= POS_F2;
                    elsif req_f1 = '1' then sig_target_pos <= POS_F1;
                    end if;
                elsif sig_current_floor = "10" then
                    if last_dir = '1' then
                        if    req_f3 = '1' then sig_target_pos <= POS_F3;
                        elsif req_f1 = '1' then sig_target_pos <= POS_F1;
                        end if;
                    else
                        if    req_f1 = '1' then sig_target_pos <= POS_F1;
                        elsif req_f3 = '1' then sig_target_pos <= POS_F3;
                        end if;
                    end if;
                end if;
            end if;
        end if;
    end process;

    -- Motor Driver
    process(clk, rst)
    begin
        if rst = '1' then
            ena_pwm <= '0'; in1_dir <= '0'; in2_dir <= '0';
        elsif rising_edge(clk) then
            if sig_motor_run = '1' then
                ena_pwm <= '1';
                if sig_motor_dir = '1' then
                    in1_dir <= '1'; in2_dir <= '0';
                else
                    in1_dir <= '0'; in2_dir <= '1';
                end if;
            else
                ena_pwm <= '0'; in1_dir <= '0'; in2_dir <= '0';
            end if;
        end if;
    end process;

    -- Position to Floor Converter
    process(clk, rst)
    begin
        if rst = '1' then
            sig_current_floor <= "01";
        elsif rising_edge(clk) then
            if    to_integer(unsigned(sig_current_pos)) < 2500 then sig_current_floor <= "01";
            elsif to_integer(unsigned(sig_current_pos)) < 7500 then sig_current_floor <= "10";
            else                                                     sig_current_floor <= "11";
            end if;
        end if;
    end process;

    -- 7-Segment Display
    process(sig_current_floor)
    begin
        case sig_current_floor is
            when "01"   => seg_out <= "1111001";
            when "10"   => seg_out <= "0100100";
            when "11"   => seg_out <= "0110000";
            when others => seg_out <= "1111111";
        end case;
    end process;

    -- UART TX Status Update
    process(clk, rst)
    begin
        if rst = '1' then
            last_floor <= "01"; tx_start <= '0';
        elsif rising_edge(clk) then
            tx_start <= '0';
            if sig_current_floor /= last_floor and tx_busy = '0' then
                last_floor <= sig_current_floor;
                if    sig_current_floor = "01" then tx_data <= x"31";
                elsif sig_current_floor = "10" then tx_data <= x"32";
                else                                 tx_data <= x"33";
                end if;
                tx_start <= '1';
            end if;
        end if;
    end process;
end Structural;
