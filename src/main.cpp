//Load monitoring system prototype.
//this code is not intended to be in production in any means.
//-ON YOUR OWN RISK- feel free to use the code in any way you want  and if you find it helpful I love to hear from you.

//WARNING : this is a big sketch to save it in the flash of an esp32 with standar partition scheme so you need to change the partition scheme first.

//Karam Dali - Damascus 02/03/2024 10:23PM



//used to fix a funny bug ;)
#define TRACE() \
  Serial.print(__FILE__);\
  Serial.print(":");\
  Serial.println(__LINE__);


//all the required libs
#include <Arduino.h>
#include <Wire.h>
#include <SparkFunTMP102.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include "time.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include "WebSocketsServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"
#include <ArduinoJson.h>
#include <Ticker.h>  


#define DEBUGE 1          //comment this line when no debuginig is needed!

//I2C slaves addresses
#define LCD_ADR 0x27      //lcd    address is 0x27
#define TMP102_ADR 0x48   //tmp102 address is 0x48

//Liqued Crystal and tmp102 sensor use I2C bus to communicate with the ESP32
#define I2C_SDA 21 
#define I2C_SCL 22

//LCD spesifications
#define LCD_ROW 2
#define LCD_COLUMN 16

#define AVG_FILTER 10 

//SDcard SPI
/*SPI is used to connect with SDcard moudule as follow 
_________________________
| SD Module  |   ESP32  |
|____________|__________|
|    MISO    |   PIN19  |
|    MOSI    |   PIN23  | 
|    SCK     |   PIN18  |
|    CS      |   PIN05  |
|____________|__________|
*/
const int MISO_PIN = 19, MOSI_PIN = 23,SCK_PIN = 18,CS_PIN=5;

//Wifi connection info
const char* ssid       = "Redmi Note 10 Pro";
const char* password   = "qwert456";

//NTP server info
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800;  //For UTC +3 (Syria) : 3 * 60 * 60 = 10800 
const int   daylightOffset_sec = 3600;

//Voltage sensor
const int VOLTAGE_PIN = 35;
const float REF_VOLTAGE = 3.6;
const float R1 = 30;            // The first ressirtor of the volatge divider.
const float R2 = 7.5;           // The seconde ressirtor of the volatge divider.

//Current sensor
const int CURRENT_PIN = 39;
const float SENSITIVITY = 0.185; // Sensitivity of the ACS712 is 0.185mV/A 
const float OFFSET = 1.6;        // the Volt we get from the sensor on 0 Amp after the voltage divider

int numberOfRecords = 0;

struct tm timeinfo;

struct sensorData {
    float temp;             //Temperature reading
    float curr;             //Current reading
    float volt;             //Voltage reading
    char timeStampe[9];     //Time of the reading
    char dateStampe[11];    //Date of the reading      
  }sensor;

struct records{             //arrays to get the last 30 reading from sensors
    float voltArry[30],curArry[30],tempArry[30];
    char dateTime[30][20];
  }chartBuffer;


                                    

AsyncWebServer server(80);                                // the server uses port 80

WebSocketsServer webSocket = WebSocketsServer(81);        // the websocket uses port 81

LiquidCrystal_I2C lcd( LCD_ADR, LCD_COLUMN, LCD_ROW);     //define LCD object (Addr - columns - rows)

TMP102 temp_sensor;                                       //define TMP102 sensor object

sqlite3 *db;                                              //sqlite database instance

Ticker repeat;                                            //used to periodically excute repeatedFunction()

//initialize all the variable needed to create JSON file
char type[12];                            //the function of the JSON file
String jsonString;                        //to store a string copy of the JSON file
JsonDocument doc;                         //JSON variable
  JsonArray time_sensor = doc["time"].to<JsonArray>();   //to save the date and time of each read from the chartBuffer
  JsonArray data_sensor = doc["data"].to<JsonArray>();   //to save 



int bufferLength = 0;     
//bool sendLiveData = false; //for expirement  - no need for it when using FreeRTOS in future versions.



/*
   LCD_message function displays a message on a LiquidCrystal_I2C display with optional scrolling.

   Parameters:
   - message: The message to be displayed on the LCD.
   - lcd: LiquidCrystal_I2C object representing the LCD display.
   - columns: The number of columns in the LCD.
   - showTime: The duration (in milliseconds) to display the message.

   Returns:
   - None

   Description:
   - Clears the LCD screen and sets the cursor to the top-left corner.
   - Computes the length of the message and checks if it's empty; if empty, returns early.
   - Calculates the ratio of message length to columns for determining if scrolling is needed.
   - If the message fits in a single row, prints it on the first row.
   - If the message is longer than the specified number of columns,
     it scrolls the message across two rows, with the first row showing the beginning
     and the second row showing the continuation.
   - Note: This implementation is intended for prototyping purposes only. It does not account
     for situations where the message requires more than two rows to be fully displayed,
     and further refinement is needed for a comprehensive solution.

*/
void LCDmessege(char message[], LiquidCrystal_I2C &lcd, int columns, int showTime);

/*
   volt() function Measure and calculate the voltage on a specified analog pin.
  
  This function employs an averaging filter to reduce noise in analog readings.
  The average value is then converted into the corresponding voltage on the ADC pin,
  considering the reference voltage (REF_VOLTAGE) and the ADC's 12-bit resolution (4095 levels).
  Finally, it calculates the voltage on the load using a voltage divider formula with resistors R1 and R2.
  
  Parameters:
   -analogPin The analog pin to measure the voltage on.

  Returns:
   - float : The voltage on the load, calculated based on the analog readings.
 */
float volt(int analogPin);

/*
   current(int analogPin) function calculates the current based on an analog reading from 
   a sensor connected to an ESP32 microcontroller.
    Algorithm:
    1. Initialize adcValue to zero for accumulating analog readings.
    2. Apply an average value filter to reduce noise in analog readings.
    3. Read the analog input from the specified pin (analogPin) multiple times (AVG_FILTER times).
    4. Calculate the average of the readings and store it in adcValue.
    5. Convert the averaged analog reading to voltage using the reference voltage (REF_VOLTAGE) and ADC resolution (4095).
    6. Calculate the current using the formula: Current = (Acs712 Offset – adcVoltage) / Sensitivity.
    7. Return the calculated current value.
  
  Parameters:
    - analogPin (int): The analog pin to which the sensor is connected on the ESP32.
  
  Returns:
    - float: The calculated current value based on the provided analog reading and sensor characteristics.
*/
float current(int analogPin);

/*
 *temperature(TMP102) function reads temperature from a TMP102 sensor using an average filter.
 * 
 *Parameters:
    - tmp A TMP102 object representing the temperature sensor.

 * Returns:
    - The average temperature in degrees Celsius obtained from multiple readings.
 */
float temperature(TMP102 tmp);

//get sensor reading
struct sensorData getSensorReading();

//save the new data in the data base
void saveSensorData(struct sensorData sensor);

//how many reacords there are in the database (runs only on startup)
int getNumberOfRecords(sqlite3* db);

//get the latest records and copy them in the buffer to update the charts (runs only on startup)
void retrieveRecords(sqlite3* db,struct records* chartBuffer);

//handl websocket events
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length);

//update the buffer after getting new data from sensors
void updateBuffer(struct records* buffer,struct sensorData sensor, int* bufferLength);

//send the buffer with date and time data to the client
void sendJsonArray(String type, float sensorValue[] , char timeValues[][25] );

//the function that excutes every 30 seconds:
//1-get new data.
//2-save the data in the data base.
//3-update buffer.
//4-broadcast the new data to all clients.
void repeatedFunction();

void setup() {

  #ifdef DEBUGE
  Serial.begin(9600);
  #endif

  //initiate LCD
  Wire.begin(I2C_SDA,I2C_SCL);
  lcd.init(I2C_SDA,I2C_SCL);
  lcd.backlight();

  //initiate SD and sqlite DB
  SPI.begin(SCK_PIN,MISO_PIN,MOSI_PIN,CS_PIN);
  SD.begin(CS_PIN);

  
  sqlite3_initialize();
  sqlite3_open("/sd/power.db", &db);
  
  #ifdef DEBUGE
    Serial.println("START Powering ESP32\0");
  #endif
  
  //initiate TMP102 sensor
  if(!temp_sensor.begin(TMP102_ADR)){
    LCDmessege("TMP102 NOT Connected\0",lcd,LCD_COLUMN,1000);

    #ifdef DEBUGE
    Serial.println("TMP102 is NOT connected. the program stopped\0");
    #endif

    while(1);
  }
  
  //NOTE: in this prototype we ignore the ALERT functionality of the TMP102 sensor
  LCDmessege("Connected to TMP102!\0",lcd,LCD_COLUMN,2000);
  temp_sensor.setExtendedMode(0);   //we only need to measure temperature in the range of -55C to +128C
  temp_sensor.setConversionRate(2); //the frequency of reading new teperature by the sensor 2 means 4Hz it's the default setting

  #ifdef DEBUGE
  Serial.println("Connected to TMP102!\0");
  #endif

  LCDmessege("Connecting to Wifi\0",lcd,LCD_COLUMN,2000);

  #ifdef DEBUGE
  Serial.printf("Connecting to %s \0", ssid);
  #endif

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  LCDmessege("Connected to WIFI\0",lcd,LCD_COLUMN,2000);
  Serial.println();
  Serial.println(WiFi.localIP());
  
  //initialize SPIFFS
  if(!SPIFFS.begin(true)){
    LCDmessege("Error has occurred while mounting SPIFFS\0",lcd,LCD_COLUMN,1000);

    #ifdef DEBUGE
    Serial.println("An Error has occurred while mounting SPIFFS. the program stopped");
    #endif

    while(1);
  }
  

  //conect to NTP to get time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if(!getLocalTime(&timeinfo)){
    LCDmessege("Failed to obtain time\0",lcd,LCD_COLUMN,2000);

    #ifdef DEBUGE
    Serial.println("Failed to obtain time\0");
    #endif


  }


  //define What the webserver needs to do
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {    
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File not found");
  });

  server.on("/sql.html", HTTP_GET, [](AsyncWebServerRequest *request) {    
    request->send(SPIFFS, "/sql.html", "text/html");          
  });


  server.serveStatic("/", SPIFFS, "/");
  
  //start websocket
  webSocket.begin();                                 
  webSocket.onEvent(webSocketEvent);

  server.begin();

  LCDmessege("The server is running\0",lcd,LCD_COLUMN,1000);
  Serial.println("The server is running\0");

  
  analogSetWidth(12);
  analogSetAttenuation(ADC_11db);

  //set the analog pin for the voltage sensore
  pinMode(VOLTAGE_PIN, INPUT);

  //set the analog pin for the current sensor
  pinMode(CURRENT_PIN, INPUT);
  
  //this is the responsible of getting values from sensors every 30 seconds
  repeat.attach(30,repeatedFunction); //repeat.attach(-seconds-,-the name of the fuction-);

  numberOfRecords = getNumberOfRecords(db);


  retrieveRecords(db,&chartBuffer);
}

void loop() {
  webSocket.loop();
}

void LCDmessege(char *message, LiquidCrystal_I2C &lcd, int columns, int showTime) {
  lcd.clear();
  lcd.setCursor(0, 0);
  int messageLength = strlen(message);
  if (messageLength == 0) {
    return;
  }

  float ratio = (float)messageLength / columns;

  if (ratio < 1.0) {
    lcd.print(message);
  } else {
    int splitIndex;
    
    for (splitIndex = columns; splitIndex >= 0 && message[splitIndex] != ' '; splitIndex--) {}
    splitIndex++; //remove white space
    int cursorPosition = 0;

    for (int i = 0; i < splitIndex; i++) {
      lcd.print(message[i]);
      lcd.setCursor(++cursorPosition, 0);
    }

    cursorPosition = 0;
    lcd.setCursor(0, 1);
    for (int i = splitIndex; i < messageLength; i++) {
      lcd.print(message[i]);
      lcd.setCursor(++cursorPosition, 1);
    }
    Wire.flush();
  }

  delay(showTime);
}

float volt(int analogPin){
  float adcValue = 0.0;

  //average value filter to filter out noise.
  int i = 0;
  while(i<AVG_FILTER){
    adcValue += analogRead(analogPin);
    i++;
  }
  adcValue = adcValue/AVG_FILTER;
  //end of the filter

  //get the voltage on the ADC pin
  float adcVoltage  = (adcValue * REF_VOLTAGE) / 4096; //REF_VOLTAGE is the voltage powering esp32 - 4095 is the ADC 12bit ADC resoulotion (2^12)
 
  //get the voltage on the load
  float inVoltage = adcVoltage*(R1+R2)/R2;
  #ifdef DEBUGE
    Serial.println("___________volt()___________");
    Serial.printf("adcVoltage = (adcValue * REF_VOLTAGE) / 4096 = (%.2f * %.1f)/4096 = %.2f\n",adcValue,REF_VOLTAGE,adcVoltage);
    Serial.printf("volt() =  adcVoltage*(R1+R2)/R2 = %.2f (%.1f - %.1f)/%.1f = %.2f\n",adcVoltage,R1,R2,R1,inVoltage);
    Serial.println("____________________________");
  #endif
  return inVoltage;
}

float current(int analogPin){
  //Current = (Acs712 Offset – (ESP32 measured analog reading)) / Sensitivity
  float adcValue = 0.0;
  //average value filter to filter out noise.
  int i = 0;
  while(i<AVG_FILTER){
    adcValue += analogRead(analogPin);
    i++;
  }
  
  adcValue = adcValue/AVG_FILTER;
  //end of the filter
  //get the voltage on the ADC pin
  float adcVoltage  = (adcValue * REF_VOLTAGE) / 4096; //REF_VOLTAGE is the voltage powering esp32 - 4095 is the ADC 12bit ADC resoulotion (2^12)
  float current = (adcVoltage-OFFSET)/SENSITIVITY;

  #ifdef DEBUGE
    Serial.println("_________current()_________");
    Serial.printf("adcVoltage = (adcValue * REF_VOLTAGE) / 4096 = (%.2f * %.1f)/4096 = %.2f\n",adcValue,REF_VOLTAGE,adcVoltage);
    Serial.printf("current() =  (adcVoltage - OFFSET) / SENSITIVITY = (%.2f - %.1f)/%.3f = %.2f\n",adcVoltage,OFFSET,SENSITIVITY,current);
    Serial.println("____________________________");
  #endif

  return current;
}

float temperature(TMP102 tmp){
  tmp.wakeup();
  float sensorRead = 0.0,isNumber = 0.0;
  int i = 0;
  while(i<AVG_FILTER){
    isNumber = tmp.readTempC();
    if(isNumber != isNumber){ //avoid NAN value
      #ifdef DEBUGE
      Serial.println("_______temperature()________");
      Serial.println("NAN");
      Serial.println("____________________________");
      #endif
      continue;
    }
    sensorRead +=isNumber;
    delay(250); //wait 250ms since the tmp102 sensor reads a new value every 4HZ ()
    i++;
    }
    sensorRead = sensorRead/AVG_FILTER;
  tmp.sleep();
  #ifdef DEBUGE
    Serial.println("_______temperature()________");
    Serial.print("Temperature = ");
    Serial.println(sensorRead);
    Serial.println("____________________________");
  #endif
  Wire.flush();
  return sensorRead;
}

struct sensorData getSensorReading(){
  //declare A stucture to save sensors readings
  struct sensorData sensor;
  sensor.temp = temperature(temp_sensor);
  sensor.curr = current(CURRENT_PIN);
  sensor.volt = volt(VOLTAGE_PIN);
  getLocalTime(&timeinfo);
  sprintf(sensor.timeStampe, "%02d:%02d:%02d\0", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  sprintf(sensor.dateStampe, "%4d-%02d-%02d\0", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday);
  #ifdef DEBUGE
    Serial.println("____getSensorReading()______");
    Serial.printf("Temperature : %.2f | Current : %.2f | Voltage : %.2f | Date : %s | Time : %s\n", sensor.temp,sensor.curr,sensor.volt,sensor.dateStampe,sensor.timeStampe);
    Serial.println("____________________________");
  #endif
  return sensor;
}

void saveSensorData(struct sensorData sensor){
  Serial.println("A");
  char sqlCommand[120];
  Serial.println("B");

  printf(sqlCommand,"INSERT INTO SensorData (date, time, voltage, current, temperature) VALUES ('%s','%s',%.2f,%.2f,%.2f);\0",sensor.dateStampe,sensor.timeStampe,sensor.volt,sensor.curr,sensor.temp);
  Serial.println(sqlCommand);
  Serial.println("C");
  int errorCheck;
  /*
  //errorCheck=sqlite3_open("/sd/power.db", &db);
  Serial.println("D");
  if (errorCheck){
    //LCDmessege("Error: Can't open database\0",lcd,LCD_COLUMN,2000);

    #ifdef DEBUGE
      Serial.println("______saveSensorData()______");
      Serial.println("Error: Can't open database");
      Serial.println("____________________________");
    #endif

    return;
  }
  */
  TRACE();
  errorCheck=sqlite3_exec(db, sqlCommand, NULL, NULL, NULL);
  TRACE();
  if (errorCheck != SQLITE_OK) {
    //LCDmessege("Error: Writting to database\0",lcd,LCD_COLUMN,2000);

    #ifdef DEBUGE
      Serial.println("______saveSensorData()______");
      Serial.println("Error: Writting to database");
      Serial.println("____________________________");
    #endif

    return;
  }
    //sqlite3_close(db);

  #ifdef DEBUGE
    Serial.println("______saveSensorData()______");
    Serial.println("Done! Data is saved into the database");
    Serial.println("____________________________");
  #endif

}

int getNumberOfRecords(sqlite3* db) {
     

    const char* query = "SELECT id FROM SensorData ORDER BY id DESC LIMIT 1;\0";

    sqlite3_stmt* statement;

    /*
    **SQLITE_API int sqlite3_prepare_v2(
    **sqlite3 *db,            // Database handle The first argument, "db", is a [database connection] obtained from a prior successful call to [sqlite3_open()], [sqlite3_open_v2()] or [sqlite3_open16()].  The database connection must not have been closed.
    **const char *zSql,       // SQL statement, UTF-8 encoded 
    **int nByte,              // Maximum length of zSql in bytes. If the nByte argument is negative, then zSql is read up to the first zero terminator.
    **sqlite3_stmt **ppStmt,  // OUT: Statement handle 
    **const char **pzTail     // OUT: Pointer to unused portion of zSql 
    **);
    */
    if (sqlite3_prepare_v2(db, query, -1, &statement, NULL) != SQLITE_OK) {
        //Handle error
        return -1; 
    }

    int lastRecordId = -1;  // Initialize to a default value

    /* ^If the SQL statement being executed returns any data, then [SQLITE_ROW]
    ** is returned each time a new row of data is ready for processing by the
    ** caller. The values may be accessed using the [column access functions].
    ** sqlite3_step(sqlite3_stmt*) is called again to retrieve the next row of data.
    */
    /*
    ^*ppStmt is left pointing to a compiled [prepared statement] that can be
    ** executed using [sqlite3_step()].  ^If there is an error, *ppStmt is set
    ** to NULL.  ^If the input text contains no SQL (if the input is an empty
    ** string or a comment) then *ppStmt is set to NULL.
    ** The calling procedure is responsible for deleting the compiled
    ** SQL statement using [sqlite3_finalize()] after it has finished with it.
    ** ppStmt may not be NULL.
    */
    if (sqlite3_step(statement) == SQLITE_ROW) {
        //SQLITE_API int sqlite3_column_int(sqlite3_stmt*, int iCol);
        //retrieve the value of the "id" column
        lastRecordId = sqlite3_column_int(statement, 0);
    }

    sqlite3_finalize(statement);
    //sqlite3_close(db);
    #ifdef DEBUGE
      Serial.println("___getNumberOfRecords()_____");
      Serial.println(lastRecordId+1);
      Serial.println("____________________________");
    #endif
    return lastRecordId;
}

void retrieveRecords(sqlite3* db,struct records* chartBuffer){
  //first check if the database has 30 records in it.
  //int numberOfRecords = getNumberOfRecords(db);
  if (numberOfRecords>30){
    numberOfRecords = 30;
  }
  Serial.print(numberOfRecords);
  char query[140];
  sprintf(query,"SELECT * FROM (SELECT id,date, time, voltage, current, temperature FROM SensorData ORDER BY id DESC LIMIT %d) AS t1 ORDER BY t1.id ASC;\0",numberOfRecords); // get the id of the last record
  sqlite3_stmt* statement;
  if (sqlite3_prepare_v2(db, query, -1, &statement, NULL) != SQLITE_OK) {
        //Handle error
        return; 
  }
  int index = 0;
  #ifdef DEBUGE
    Serial.println("_____retrieveRecords()______");
  #endif
  int i =0;
  while (sqlite3_step(statement) == SQLITE_ROW){
    //index = sqlite3_column_int(statement, 0);
    const char* date = (const char*)sqlite3_column_text(statement, 1);
    const char* time = (const char*)sqlite3_column_text(statement, 2);
    float voltage = sqlite3_column_double(statement, 3);
    float current = sqlite3_column_double(statement, 4);
    float temperature = sqlite3_column_double(statement, 5);

    #ifdef DEBUGE
      Serial.printf("%d : Data from database -> date: %s | time : %s | voltage : %f | current : %f | temperature : %f\n",i,date,time,voltage,current,temperature);
    #endif 
    chartBuffer->voltArry[index] = voltage;
    chartBuffer->curArry[index] = current;
    chartBuffer->tempArry[index] = temperature;
    sprintf(chartBuffer->dateTime[index],"%s %s", date, time);
    #ifdef DEBUGE
      Serial.printf("%d : Data saved in the buffer -> dateTime: %s | voltage : %f | current : %f | temperature : %f\n",i,chartBuffer->dateTime[index],chartBuffer->voltArry[index],chartBuffer->curArry[index] ,chartBuffer->tempArry[index]);
    #endif

    index++;
    i++;
  }
  Serial.print("index : ");
  Serial.println(index);
  #ifdef DEBUGE
    Serial.println("____________________________");
  #endif

  sqlite3_finalize(statement);
  //sqlite3_close(db);
}

void updateBuffer(struct records* buffer,struct sensorData sensor){

  #ifdef DEBUGE
    Serial.println("______updateBuffer()________");
  #endif
  if (numberOfRecords>30){
    numberOfRecords = 30;
  }
  Serial.print("numberOfRecords = ");
  Serial.println(numberOfRecords);
  Serial.print("29-(30-(numberOfRecords)) = ");
  Serial.println(29-(30-(numberOfRecords)));
  for (int i = 0;i<29-(30-(numberOfRecords));i++){
    buffer->curArry[i]= buffer->curArry[i+1];
    buffer->voltArry[i] = buffer->voltArry[i+1];
    buffer->tempArry[i] = buffer->tempArry[i+1];
    strncpy(buffer->dateTime[i], buffer->dateTime[i + 1], 20);
    #ifdef DEBUGE
      Serial.printf("%d : dateTime: %s | voltage : %f | current : %f | temperature : %f\n",i,buffer->dateTime[i],buffer->voltArry[i],buffer->curArry[i] ,buffer->tempArry[i]);
    #endif
  }
  buffer->curArry[numberOfRecords-1]= sensor.curr;
  buffer->voltArry[numberOfRecords-1] = sensor.volt;
  buffer->tempArry[numberOfRecords-1] = sensor.temp;
  snprintf(buffer->dateTime[numberOfRecords-1], sizeof(buffer->dateTime[numberOfRecords-1]), "%s %s\0", sensor.dateStampe, sensor.timeStampe);
  #ifdef DEBUGE
    Serial.printf("%d : dateTime: %s | voltage : %f | current : %f | temperature : %f\n",numberOfRecords-1,buffer->dateTime[numberOfRecords-1],buffer->voltArry[numberOfRecords-1],buffer->curArry[numberOfRecords-1] ,buffer->tempArry[numberOfRecords-1]);
    Serial.println("____________________________");
  #endif
}

void sendJsonArray(String type, float sensorValue[] , char timeValues[][20] ,int bufferLength) {
  String jsonString;
  JsonDocument doc;
  JsonArray time_sensor = doc["time"].to<JsonArray>();
  JsonArray data_sensor = doc["data"].to<JsonArray>();
  for (int i = 0; i < 30; i++) {
    data_sensor.add(sensorValue[i]);
    time_sensor.add(timeValues[i]);
  }
  doc["type"] = type;
  serializeJson(doc, jsonString);     // convert JSON object to a string
  webSocket.broadcastTXT(jsonString); // send JSON string to all clients
}

void repeatedFunction(){
  sensor = getSensorReading();              //read data from the sensors.
  saveSensorData(sensor);                   //save the data in the sqlite database.
  numberOfRecords = getNumberOfRecords(db); //get the number of record stored in the buffer.
  updateBuffer(&chartBuffer,sensor);        //update the puffer with latest gathered data
  sendJsonArray("update_volt", chartBuffer.voltArry , chartBuffer.dateTime ,numberOfRecords);
  sendJsonArray("update_curr", chartBuffer.curArry , chartBuffer.dateTime ,numberOfRecords);
  sendJsonArray("update_temp", chartBuffer.tempArry , chartBuffer.dateTime ,numberOfRecords);
}

//function forms the needed sql statment to retrive historical data according to user's chossen dates from the html page.
String getSQLquery(uint8_t * payload){
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }
  const char* from = doc["from"];
  const char* to = doc["to"];
  String sql = "select * from SensorData where date between '";
  sql +=(String)from;
  sql +="' and '";
  sql +=(String)to;
  sql +="';";
  #ifdef DEBUGE
    Serial.println("______webSocketEvent()_______");
    Serial.print("SQL command = "); 
    Serial.println(sql.c_str());
    Serial.println("____________________________");
  #endif
  return sql;
}

//function that take the same arguments and it is excuted when the websockent will handl an events.
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {    
  switch (type) {                                     //switch on the type of information sent
    case WStype_DISCONNECTED:                         //if a client is disconnected then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            //if a client is connected then type == WStype_CONNECTED
      sendJsonArray("update_volt", chartBuffer.voltArry , chartBuffer.dateTime ,numberOfRecords);
      sendJsonArray("update_curr", chartBuffer.curArry , chartBuffer.dateTime ,numberOfRecords);
      sendJsonArray("update_temp", chartBuffer.tempArry , chartBuffer.dateTime ,numberOfRecords);
      Serial.println("Client " + String(num) + " connected");
      break;
    case WStype_TEXT: 
      String sql =  getSQLquery(payload);
      sqlite3_stmt* statement;
      if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, NULL) != SQLITE_OK) {
        //Handle error
        return; 
      }
      while (sqlite3_step(statement) == SQLITE_ROW){
        int id = sqlite3_column_int(statement,0);
        const char* date = (const char*)sqlite3_column_text(statement, 1);
        const char* time = (const char*)sqlite3_column_text(statement, 2);
        float voltage = sqlite3_column_double(statement, 3);
        float current = sqlite3_column_double(statement, 4);
        float temperature = sqlite3_column_double(statement, 5);
        JsonDocument doc;
        doc["type"] = "query";
        doc["id"] = id;
        doc["date"] = date;
        doc["time"] = time;
        doc["volt"] = voltage;
        doc["curr"] = current;
        doc["temp"] = temperature;
        String jsonString;
        serializeJson(doc, jsonString);
        webSocket.sendTXT(num,jsonString);
      }
  }
}