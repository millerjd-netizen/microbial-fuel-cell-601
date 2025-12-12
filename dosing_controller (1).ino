// Recalibrated: Automated Dosing Control Logic

// Arduino GIGA R1

// ======================================================

// New Logic: Implements a CRITICAL EC Check (Salt Toxicity Guardrail)
// to prevent dosing that would worsen a lethal salt crisis.

// ------------ Pump Pins (Active HIGH) ------------
const int PUMP_SODIUM_BICARB   = 2; // For raising pH (Increases EC)
const int PUMP_CITRIC_ACID     = 3; // For lowering pH
const int PUMP_DEIONIZED_WATER = 4; // For lowering EC / raising ORP (Dilution)
const int PUMP_MOLASSES        = 5; // For lowering ORP / raising EC (Food)
const int PUMP_URINE           = 6; // For raising EC / pH (Nutrient)
const int PUMP_SPIRULINA       = 7; // For raising EC / pH (Nutrient)

const unsigned long DOSING_PULSE_MS = 1000;      // Dosing activation time: 1 second
const unsigned long DOSING_INTERVAL_MS = 300000; // Dosing frequency: 5 minutes (300,000 ms)

// ------------ Sensor Pins ------------
const int PH_PIN   = A0;
const int ORP_PIN  = A1;
const int EC_PIN   = A2;
const int MFC_PIN  = A3; // MFC is read but ignored for control logic

// ------------ System Target Ranges ------------
// **CRITICAL: ADJUST THESE VALUES BASED ON YOUR BIOLOGICAL OPTIMA**
const float TARGET_PH_L = 6.8;
const float TARGET_PH_H = 7.2;
const float TARGET_EC_L = 4.0; // mS/cm
const float TARGET_EC_H = 5.0; // mS/cm
// NEW CRITICAL SAFETY LIMIT
const float CRITICAL_EC_H = 10.0; // mS/cm: Above this level, halt all dosing to prevent salt poisoning.

// SWAPPED L and H to reflect that -250mV is 'lower' than -150mV
const float TARGET_ORP_L = -250.0; // mV  <-- Should be the MOST negative limit (healthy)
const float TARGET_ORP_H = -150.0; // mV  <-- Should be the LEAST negative limit (stressed)

// ------------ Calibration Constants ------------
const float VREF = 3.3;
const int   ADC_MAX = 4095;
const float PH_SLOPE     = -5.59047;
const float PH_INTERCEPT = 15.835;
const float EC_SLOPE_MSPERMV = 0.00864;
const float EC_OFFSET_MS     = 0.033;
const float ORP_OFFSET = -1524.0;
const int ORP_OVERSAMPLES = 50;

// ------------ Timing Variables ------------
unsigned long lastDosingTime = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 5000; // Read sensors every 5 seconds

// ------------ Forward Declarations ------------
struct SensorData {
  float pH;
  float EC_mS;
  float ORP_mV;
  float MFC_mV;
};

SensorData readSensors();
void readAndPrintSensors();
void executeControlLogic();
void Actuate_Pump(int pumpPin, const char* message);

// ======================================================
void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("Starting Automated Dosing Control...");
  Serial.println("EC CRITICAL LIMIT SET: 10.0 mS/cm (Dosing will halt above this!)");
  
  analogReadResolution(12);
  
  // Initialize all pump pins
  pinMode(PUMP_SODIUM_BICARB, OUTPUT);
  pinMode(PUMP_CITRIC_ACID, OUTPUT);
  pinMode(PUMP_DEIONIZED_WATER, OUTPUT);
  pinMode(PUMP_MOLASSES, OUTPUT);
  pinMode(PUMP_URINE, OUTPUT);
  pinMode(PUMP_SPIRULINA, OUTPUT);

  // Ensure all pumps are OFF
  digitalWrite(PUMP_SODIUM_BICARB, LOW);
  digitalWrite(PUMP_CITRIC_ACID, LOW);
  digitalWrite(PUMP_DEIONIZED_WATER, LOW);
  digitalWrite(PUMP_MOLASSES, LOW);
  digitalWrite(PUMP_URINE, LOW);
  digitalWrite(PUMP_SPIRULINA, LOW);
  
  lastDosingTime = millis() - DOSING_INTERVAL_MS; // Force first dosing attempt immediately
}

// ======================================================
void loop() {
  unsigned long currentTime = millis();
  
  // 1. Sensor Reading Loop (High Frequency)
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    readAndPrintSensors();
    lastSensorRead = currentTime;
  }
  
  // 2. Dosing Control Loop (Low Frequency: Once every 5 minutes)
  if (currentTime - lastDosingTime >= DOSING_INTERVAL_MS) {
    executeControlLogic();
    lastDosingTime = currentTime;
  }
}

// ======================================================
// Sensor Reading Function (Used by both loops)
// ======================================================
SensorData readSensors() {
  SensorData data;
  
  // ---------- pH ----------
  int ph_raw = analogRead(PH_PIN);
  float ph_voltage = ph_raw * (VREF / ADC_MAX);
  data.pH = PH_SLOPE * ph_voltage + PH_INTERCEPT;
  
  // ---------- ORP (simple offset only) ----------
  long sum = 0;
  for (int i = 0; i < ORP_OVERSAMPLES; i++) {
    sum += analogRead(ORP_PIN);
    delayMicroseconds(100);
  }
  float orp_raw = sum / (float)ORP_OVERSAMPLES;
  float orp_mV = (orp_raw * VREF / ADC_MAX) * 1000.0;
  data.ORP_mV = orp_mV + ORP_OFFSET;
  
  // ---------- EC ----------
  int ec_raw = analogRead(EC_PIN);
  float ec_mV = (float)ec_raw * 3300.0 / 4095.0;
  data.EC_mS = EC_SLOPE_MSPERMV * ec_mV + EC_OFFSET_MS;
  
  // ---------- MFC Voltage ----------
  int mfc_raw = analogRead(MFC_PIN);
  data.MFC_mV = (float)mfc_raw * 3300.0 / 4095.0;
  
  return data;
}

void readAndPrintSensors() {
  SensorData data = readSensors();
  Serial.print("pH: ");
  Serial.print(data.pH, 2);
  Serial.print("   |   ORP_mV: ");
  Serial.print(data.ORP_mV, 1);
  Serial.print("   |   EC_mS: ");
  Serial.print(data.EC_mS, 3);
  Serial.print("   |   MFC_mV: ");
  Serial.println(data.MFC_mV, 1);
}

// ======================================================
// Dosing Logic Function
// ======================================================
void executeControlLogic() {
  SensorData data = readSensors(); // Read fresh data for decision
  
  Serial.println("--- Starting Dosing Check ---");

  // CRITICAL SAFETY CHECK (Priority 0: Halt all dosing if EC is lethally high)
  if (data.EC_mS > CRITICAL_EC_H) {
    Serial.println("!!! CRITICAL EC WARNING !!!");
    Serial.print("EC (");
    Serial.print(data.EC_mS, 2);
    Serial.print(" mS/cm) is ABOVE LETHAL LIMIT (");
    Serial.print(CRITICAL_EC_H, 1);
    Serial.println(" mS/cm). HALTING ALL DOSING.");
    Serial.println("Manual intervention is REQUIRED to dilute contents.");
    return; // HALT AND DO NOT DOSE ANYTHING
  }


  // PH CONTROL (Priority 1: Direct action, minimal side effects)
  if (data.pH < TARGET_PH_L) {
    // Too Acidic: Use Sodium Bicarbonate (Note: this raises EC, see CRITICAL EC check above)
    Actuate_Pump(PUMP_SODIUM_BICARB, "pH LOW: Adding Sodium Bicarbonate");
    return; // Exit after 1 dose
  } else if (data.pH > TARGET_PH_H) {
    // Too Basic: Use Citric Acid
    Actuate_Pump(PUMP_CITRIC_ACID, "pH HIGH: Adding Citric Acid");
    return; // Exit after 1 dose
  }

  // EC CONTROL (Priority 2: Nutrient/Salinity)
  if (data.EC_mS < TARGET_EC_L) {
    // Too Dilute/Low Nutrients: Cross-Correction Logic
    if (data.pH < TARGET_PH_H - 0.1) { // pH on the lower side, favors UP inputs
      // Use Spirulina for EC UP & pH UP
      Actuate_Pump(PUMP_SPIRULINA, "EC LOW: Adding Powdered Spirulina (to also raise pH)");
    } else {
      // Use Urine for general nutrient/EC boost
      Actuate_Pump(PUMP_URINE, "EC LOW: Adding Urine");
    }
    return; // Exit after 1 dose
  } else if (data.EC_mS > TARGET_EC_H) {
    // Too Concentrated (but not critically): Use Deionized Water
    Actuate_Pump(PUMP_DEIONIZED_WATER, "EC HIGH: Adding Deionized Water (to lower EC)");
    return; // Exit after 1 dose
  }

  // ORP CONTROL (Priority 3: Oxidation/Reduction Balance)
  if (data.ORP_mV < TARGET_ORP_L) {
    // Too Reducing/Anaerobic: Need to raise ORP. Cross-Correction Logic
    if (data.EC_mS > TARGET_EC_L + 0.1) { // EC is stable/high, favors DI water (lowers EC)
      Actuate_Pump(PUMP_DEIONIZED_WATER, "ORP LOW: Adding Deionized Water (to also lower EC)");
    } else {
      // Dilute minimally as a primary action to raise ORP/reduce toxicity
      Actuate_Pump(PUMP_DEIONIZED_WATER, "ORP LOW: Adding small DI Water dose");
    }
    return; // Exit after 1 dose
  } else if (data.ORP_mV > TARGET_ORP_H) {
    // Too Oxidizing/Aerobic: Need reducing agent (organic load). Cross-Correction Logic
    if (data.EC_mS < TARGET_EC_H - 0.1) { // EC is stable/low, favors Molasses (raises EC)
      Actuate_Pump(PUMP_MOLASSES, "ORP HIGH: Adding Molasses (to also raise EC)");
    } else {
      // Default ORP reduction via Molasses
      Actuate_Pump(PUMP_MOLASSES, "ORP HIGH: Adding Molasses");
    }
    return; // Exit after 1 dose
  }

  // All parameters are within range
  Serial.println("System is Stable. No Dosing Required.");
}

// ======================================================
// Pump Activation Helper Function
// ======================================================
void Actuate_Pump(int pumpPin, const char* message) {
  Serial.print("DOSING: ");
  Serial.println(message);
  
  digitalWrite(pumpPin, HIGH);
  delay(DOSING_PULSE_MS); // Pump runs for 1 second
  digitalWrite(pumpPin, LOW);
  
  Serial.println("DOSING COMPLETE.");
}