

int lcd_init (void);   // initialize lcd

int lcd_send_cmd (char cmd);  // send command to the lcd

int lcd_send_data (char data);  // send data to the lcd

int lcd_send_string (char *str);  // send string to the lcd

int lcd_put_cur(int row, int col);  // put cursor at the entered position row (0 or 1), col (0-15);

int lcd_clear (void);

