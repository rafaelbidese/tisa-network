void setup()
{
    Serial.begin(9600);
    analogReference(INTERNAL); //an built-in reference, equal to 1.1 volts
}

void loop()
{
    int value = analogRead(A0);
    float voltage = value*1.1/1023;
    float temp = voltage*100; // 10mV/C
    Serial.println(temp);

    delay(1000);
}