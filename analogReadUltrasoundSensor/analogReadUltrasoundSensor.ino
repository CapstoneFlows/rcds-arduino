int sensorPin = 2;
int ledPin = 4;
int sensorValue = 0;
// Used for 10 cm
int trigPin = 3;

long duration;
int distance;

// ADC maps 0V to 5V as 0 to 1023
// Power provides some capacitance because no ground (floating); need to adjust ADC reads for floating voltage

void setup() {
  // put your setup code here, to run once:
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  pinMode(trigPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(sensorPin, HIGH);
  distance = duration*0.034/2;
  
  Serial.println(distance);
}
