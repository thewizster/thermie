// Include the ESP8266 WiFi library. (Works a lot like the
// Arduino WiFi library.)
#include <ESP8266WiFi.h>
// Include the I2C library
#include <Wire.h>

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "your_wifi_ssid_here";
const char WiFiPSK[] = "your_wifi_pass_here";

/////////////////////
// Pin Definitions //
/////////////////////
const int LED_PIN = 5; // Thing's onboard LED
const int ANALOG_PIN = A0; // The only analog pin on the Thing
const int DIGITAL_PIN = 12; // Digital pin to be read

/////////////////
// THERMIE API //
/////////////////
const char Id[] = "OfficeThermie";
const char Version[] = "1.0.1";
const char CloudHost[] = "data.example.com";
const char PublicKey[] = "your_pub_key_here";
const char PrivateKey[] = "your_private_key_here";
int state = 0; // off

///////////////////////////////
// I2C TMP102 sensor related //
///////////////////////////////
int tmp102Address = 0x48;
float celsius;
float fahrenheit;
float setPointF = 74;

/////////////////
// Post Timing //
/////////////////
const unsigned long postRate = 15000;
unsigned long lastPost = 0;

/////////////////
// HTTP Server //
/////////////////
WiFiServer server(80);

void setup() 
{
  initHardware(); // Setup input/output I/O pins
  connectWiFi(); // Connect to WiFi
  
  server.begin();
  Serial.printf("Web server started, open %s in a web browser\n", WiFi.localIP().toString().c_str());
  
  Wire.begin();
  digitalWrite(LED_PIN, LOW); // LED on to indicate connect success
}

void loop() 
{
    // This conditional will execute every lastPost milliseconds
    if ((lastPost + postRate <= millis()) || lastPost == 0)
    {
        celsius = getTemperature();
        Serial.print("Celsius: ");
        Serial.println(celsius);
        fahrenheit = (1.8 * celsius) + 32;
        Serial.print("Fahrenheit: ");
        Serial.println(fahrenheit);

        Serial.println("Checking temperature");
        if (tryTriggerTemp())
        {
        lastPost = millis();
        //Serial.println("Post Suceeded! temp is lower than setPoint");
        }
    }

    WiFiClient client = server.available();
    if (!client)
    {
        return;
    }

    Serial.println("\n[Client connected]");
    // Wait until the client sends some data
    while(!client.available()){
        delay(1);
    }

    String line = client.readStringUntil('\r');
    Serial.print(line);
    client.flush();

    // Match the request
    int val; // flag request type
    if (line.indexOf("/api/setpoint+") != -1){
        Serial.println("[API SETPOINT +]");
        val = 0;
        setPointF++;
        client.println(apiDataResponse());
    }
    else if (line.indexOf("/api/setpoint-") != -1){
        Serial.println("[API SETPOINT -]");
        val = 1;
        setPointF--;
        client.println(apiDataResponse());
    }
    else if (line.indexOf("/api/data") != -1){
        Serial.println("[API DATA]");
        val = 2;
        client.println(apiDataResponse());
    }
    // wait for end of client's request, that is marked with an empty line
    else
    {
        Serial.println("[WEB PAGE REQUEST]");
        val = 3;
        client.println(prepareHtmlPage());
    }

    // close the connection:
    client.stop();
    Serial.println("[Client disonnected]");
}

void connectWiFi()
{
  byte ledStatus = LOW;
  Serial.println();
  Serial.println("Connecting to: " + String(WiFiSSID));
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);

  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(LED_PIN, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void initHardware()
{
  Serial.begin(9600);
  pinMode(DIGITAL_PIN, INPUT_PULLUP); // Setup an input to read
  pinMode(LED_PIN, OUTPUT); // Set LED as output
  digitalWrite(LED_PIN, HIGH); // LED off
  // Don't need to set ANALOG_PIN as input, 
  // that's all it can be.
}

int tryTriggerTemp()
{
  if (fahrenheit < setPointF)
  {
    // temp is lower
    digitalWrite(LED_PIN, LOW);
    state = 1; // ON
    Serial.println("Low temperature detected.");
  } else {
    digitalWrite(LED_PIN, HIGH);
    state = 0; // OFF
    Serial.println("Normal temperature.");
  }
  
  return 1;
}

float getTemperature(){
  Wire.requestFrom(tmp102Address,2); 

  byte MSB = Wire.read();
  byte LSB = Wire.read();

  //it's a 12bit int, using two's compliment for negative
  int TemperatureSum = ((MSB << 8) | LSB) >> 4; 

  float celsius = TemperatureSum*0.0625;
  return celsius;
}

// prepare a web page to be send to a client (web browser)
String prepareHtmlPage()
{
  String htmlPage = 
     String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "Connection: close\r\n" +  // the connection will be closed after completion of the response
            "\r\n" +
            "<!DOCTYPE HTML>" +
            "<head>" +
            "</head>" +
            "<html>" +
            "<div id='app' data-version='" + String(Version) + "' data-apiurl='http://" + WiFi.localIP().toString().c_str() + "/'>" +
            "<h1>{{ message }}</h1>" +
            "<p data-setpoint='" + String(setPointF) + "'>Set Point (F): " + String(setPointF) + "</p>" +
            "<p data-temperature='" + String(fahrenheit) + "'>Temperature (F): " + String(fahrenheit) + "</p>" +
            "</div>" +
            "<script src='https://unpkg.com/vue/dist/vue.js'></script>" +
            "<script>new Vue({el: '#app', data: {message: 'Hello, I am Thermie!'}})</script>" +
            "</html>" +
            "\r\n";
  return htmlPage;
}

String apiTemperatureResponseF()
{
  String response =
     String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "Connection: close\r\n" +  // the connection will be closed after completion of the response
            "\r\n" +
            String(fahrenheit);
  return response;
}

String apiTemperatureResponseC()
{
  String response =
     String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "Connection: close\r\n" +  // the connection will be closed after completion of the response
            "\r\n" +
            String(celsius);
  return response;
}

String compileJsonData()
{    
  // assemble JSON data
  String data =
    String("{") + 
    "\"id\": \"" + String(Id) + "\"," +
    "\"source\": \"http://" + WiFi.localIP().toString().c_str() + "\"," +
    "\"version\": \"" + String(Version) + "\"," +
    "\"state\": \"" + String(state) + "\"," +
    "\"tempf\": " + String(fahrenheit) + "," +
    "\"setpointf\": " + String(setPointF) + "" +
    "}";
  return data;
}

String apiDataResponse()
{
  String response =
     String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/json\r\n" +
            "Connection: close\r\n" +  // the connection will be closed after completion of the response
            "\r\n" +
            String(compileJsonData());
  return response;
}
