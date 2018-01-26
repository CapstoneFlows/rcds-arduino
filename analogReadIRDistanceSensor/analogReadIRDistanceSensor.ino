int sensorPin = A1;
int sensorValue = 0;
int outputValue = 0;


// ADC maps 0V to 5V as 0 to 1023
// Sensor maps 10 cm to 80 cm as 3.1 V to 0.4 V
// Call valid range 0.5 V to 1.5 V
// Power provides some capacitance because no ground (floating); need to adjust ADC reads for floating voltage

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  sensorValue = analogRead(sensorPin);
  outputValue = 30431 * pow(sensorValue, -1.169); // centimeters
  
  Serial.println(outputValue);
}
