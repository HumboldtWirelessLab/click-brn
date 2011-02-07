src::FromSocket("UDP", 127.0.0.1, 8086);

src
-> Print("Data",48,TIMESTAMP true)
-> ToDump("data.dump");
