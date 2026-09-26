#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// =====================================================
// CONFIGURACIÓN DE PINES (PINES 8, 9 Y 10 LIBRES)
// =====================================================

// Módulo RFID MFRC522 (Interfaz SPI)
#define PIN_SS           6   // SDA
#define PIN_RST          7   // Reset

// Sensor Ultrasónico HC-SR04
const int pinTrig      = 2;
const int pinEcho      = 3;

// Servomotor (PWM)
const int pinServo     = 5;  

// Módulo Bluetooth HC-05 / HC-06
const int pinBT_RX     = A0; // Arduino RX <- TXD del Bluetooth
const int pinBT_TX     = A1; // Arduino TX -> RXD del Bluetooth


// =====================================================
// CONFIGURACIÓN DEL SISTEMA DE PEAJE
// =====================================================

const float DISTANCIA_ACTIVACION = 15.0;       // Detección estandarizada a 15 cm
const unsigned long TIEMPO_ESPERA_PASO = 4000; // Barrera abierta por 4 segundos

const int SERVO_CERRADO = 0;
const int SERVO_ABIERTO = 90;

bool vehiculoPresente = false;


// =====================================================
// OBJETOS
// =====================================================

MFRC522 rfid(PIN_SS, PIN_RST);
Servo miServo;
SoftwareSerial btSerial(pinBT_RX, pinBT_TX);


// =====================================================
// FUNCIONES AUXILIARES
// =====================================================

// Medición de distancia con el HC-SR04
float medirDistanciaEstandar() {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);

  // Timeout ajustado a 15ms (~250 cm de alcance máximo)
  long duracion = pulseIn(pinEcho, HIGH, 15000);

  if (duracion == 0) {
    return -1.0; // Sin eco / Fuera de rango
  }

  return (duracion * 0.0343) / 2.0;
}

// Control del movimiento de la pluma/barrera
void abrirBarrera(String metodoApertura) {
  Serial.println();
  Serial.println("=========================================");
  Serial.print("   ACCESO AUTORIZADO POR: ");
  Serial.println(metodoApertura);
  Serial.println("=========================================");

  btSerial.print("ACCESO_PERMITIDO:");
  btSerial.println(metodoApertura);

  Serial.println("Abriendo barrera...");
  miServo.write(SERVO_ABIERTO);
  delay(600);

  Serial.print("Tiempo de paso: ");
  Serial.print(TIEMPO_ESPERA_PASO / 1000);
  Serial.println(" segundos.");

  delay(TIEMPO_ESPERA_PASO);

  Serial.println("Cerrando barrera...");
  miServo.write(SERVO_CERRADO);
  delay(800);

  Serial.println("Barrera cerrada. Esperando siguiente vehículo...");
  Serial.println();
  
  vehiculoPresente = false; // Reiniciar estado de presencia
}

// Lectura de tarjetas RFID
bool verificarRFID() {
  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }

  // Imprimir el UID de la tarjeta en el Monitor Serial
  Serial.print(">> TARJETA REGISTRADA -> UID:");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;
}


// =====================================================
// CONFIGURACIÓN INICIAL (SETUP)
// =====================================================

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  // Inicializar bus SPI y el lector RFID
  SPI.begin();
  rfid.PCD_Init();

  // Pines ultrasónico
  pinMode(pinTrig, OUTPUT);
  pinMode(pinEcho, INPUT);

  // Configurar Servomotor
  miServo.attach(pinServo, 544, 2400);
  miServo.write(SERVO_CERRADO);

  delay(500);

  Serial.println("=========================================");
  Serial.println("  PEAJE AUTOMATIZADO CON CONTROL RFID");
  Serial.println("=========================================");
  
  // Diagnóstico automático del lector RFID
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print("Estado MFRC522: ");
  if (v == 0x91 || v == 0x92) {
    Serial.println("¡Módulo detectado y listo!");
  } else {
    Serial.println("ADVERTENCIA: Revisa conexiones SPI (Pines 6, 7, 11, 12, 13 y 3.3V)");
  }
  
  Serial.println("Lógica: Ultrasónico detecta llegada. RFID abre la barrera.");
  Serial.println("Esperando vehículos...");
  Serial.println();
}


// =====================================================
// BUCLE PRINCIPAL (LOOP)
// =====================================================

void loop() {

  // 1. Detección de vehículo con Ultrasónico (SOLO INFORMA PRESENCIA)
  float distancia = medirDistanciaEstandar();

  if (distancia > 0 && distancia <= DISTANCIA_ACTIVACION) {
    if (!vehiculoPresente) {
      vehiculoPresente = true;
      Serial.println("-----------------------------------------");
      Serial.print("VEHÍCULO DETECTADO A ");
      Serial.print(distancia, 1);
      Serial.println(" cm.");
      Serial.println("Por favor, Acerque su tarjeta RFID...");
      Serial.println("-----------------------------------------");
    }
  } else {
    vehiculoPresente = false;
  }

  // 2. Lectura RFID (ÚNICO DISPARADOR DE APERTURA DEL SERVO)
  if (verificarRFID()) {
    abrirBarrera("TARJETA RFID");
    return;
  }

  // 3. Control Bluetooth (Apertura manual por App)
  if (btSerial.available()) {
    char dato = btSerial.read();
    if (dato == 'A' || dato == 'a') {
      abrirBarrera("COMANDO BLUETOOTH");
    }
  }

  delay(100);
}