float led_frecuencia_hz =15.0f;
float half_period_ms;
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  float half_period_ms= 1000/(2*(led_frecuencia_hz)); //porque delay  es ms
  Serial.println("Ingrese la frecuencia en Hz:");
}


void loop() {

  // Revisar si llegó una nueva frecuencia por Serial
  if (Serial.available() > 1) {

    float nueva_frecuencia = Serial.parseFloat();

    if (nueva_frecuencia >= 1) {
      led_frecuencia_hz = nueva_frecuencia;

      half_period_ms = 1000.0 / (2.0 * led_frecuencia_hz);

      Serial.print("Nueva frecuencia: ");
      Serial.print(led_frecuencia_hz);
      Serial.println(" Hz");

      Serial.print("Periodo: ");
      Serial.print(1000.0 / led_frecuencia_hz);
      Serial.println(" ms");
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
  delay(half_period_ms);
  digitalWrite(LED_BUILTIN, LOW);
  delay(half_period_ms);
}