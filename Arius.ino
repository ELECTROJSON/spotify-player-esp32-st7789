// ============================================================
// Spotify Player for ESP32 S3 with ST7789 Display
// Created by: JSON<3
// License: OPEN SOURCE (Free to use, modify and share)
// Year: 2026
// ============================================================
// Description:
// WiFi-enabled Spotify "Now Playing" display.
// Shows album art, track title, artist and progress bar.
// Configuration via web browser (AP mode).
// ============================================================


#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include <base64.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "logo.h"


#define CONFIG_BUTTON 0 

Preferences preferences;
WebServer server(80);

String wifi_ssid = "";
String wifi_password = "";
String spotify_client_id = "";
String spotify_client_secret = "";
String spotify_refresh_token = "";
bool scroll_title = true;
int scroll_speed = 200;
int update_interval = 5;
int brightness = 180;
bool configMode = false;
bool buttonPressed = false;
unsigned long pressStartTime = 0;
String scrollingBuffer = "";
int scrollPosition = 0;
unsigned long lastScrollTime = 0;
String lastScrolledTitle = "";
String accessToken = "";
unsigned long tokenExpires = 0;
String title = "";
String artist = "";
String imageUrl = "";
int progressMs = 0;
int durationMs = 1;
bool isPlaying = false;
bool wifiConnected = false;

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  LGFX()
  {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 3;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 20000000;
      cfg.pin_sclk = 8;
      cfg.pin_mosi = 21;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();
      cfg.pin_cs = 16;
      cfg.pin_rst = 4;
      cfg.panel_width = 240;
      cfg.panel_height = 280;
      cfg.offset_x = 0;
      cfg.offset_y = 20;
      cfg.invert = true;
      cfg.rgb_order = false;
      _panel.config(cfg);
    }

    {
      auto cfg = _light.config();
      cfg.pin_bl = 6;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};

static LGFX lcd;
void startAPMode();
void startNormalMode();
bool refreshAccessToken();
void drawScreen(bool forceRedraw = true);
void updateProgress();
void checkButton();
void loadConfig();
void saveConfig();
void drawTextOverlay(bool forceRedraw = false);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  lcd.pushImage(x, y, w, h, bitmap);
  return true;
}

void checkButton()
{
  static unsigned long lastDebounceTime = 0;
  static int lastState = HIGH;
  int reading = digitalRead(CONFIG_BUTTON);
  
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (reading == LOW && lastState == HIGH) {
      buttonPressed = true;
      pressStartTime = millis();
    }
    
    if (reading == HIGH && lastState == LOW) {
      buttonPressed = false;
      pressStartTime = 0;
    }
  }
  
  lastState = reading;
  
  if (buttonPressed && pressStartTime > 0 && (millis() - pressStartTime) > 3000) {
    lcd.fillScreen(lcd.color565(50, 50, 50));
    lcd.setTextColor(lcd.color565(255, 255, 255));
    lcd.setTextSize(2);
    lcd.setCursor(20, 120);
    lcd.print("AP Mode");
    lcd.setCursor(20, 150);
    lcd.print("Configuration...");
    delay(2000);
    
    configMode = true;
    buttonPressed = false;
    pressStartTime = 0;
    startAPMode();
  }
}


void loadConfig()
{
  preferences.begin("spotify", false);
  
  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_password = preferences.getString("wifi_password", "");
  spotify_client_id = preferences.getString("client_id", "");
  spotify_client_secret = preferences.getString("client_secret", "");
  spotify_refresh_token = preferences.getString("refresh_token", "");
  scroll_title = preferences.getBool("scroll", true);
  scroll_speed = preferences.getInt("scroll_speed", 200);
  update_interval = preferences.getInt("update_int", 5);
  brightness = preferences.getInt("brightness", 180);
  
  preferences.end();
  
  if (wifi_ssid.length() == 0 || spotify_client_id.length() == 0) {
    configMode = true;
  }
}

void saveConfig()
{
  preferences.begin("spotify", false);
  
  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_password", wifi_password);
  preferences.putString("client_id", spotify_client_id);
  preferences.putString("client_secret", spotify_client_secret);
  preferences.putString("refresh_token", spotify_refresh_token);
  preferences.putBool("scroll", scroll_title);
  preferences.putInt("scroll_speed", scroll_speed);
  preferences.putInt("update_int", update_interval);
  preferences.putInt("brightness", brightness);
  
  preferences.end();
  
  lcd.setBrightness(brightness);
}


String getConfigPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <title>Spotify Player Configuration</title>
    <style>
        body { font-family: Arial; padding: 20px; background: #f0f0f0; }
        .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; }
        h2 { color: #1DB954; text-align: center; }
        h3 { margin-top: 25px; color: #333; border-bottom: 1px solid #ddd; padding-bottom: 5px; }
        label { display: block; margin: 10px 0 5px; font-weight: bold; }
        input, select { width: 100%; padding: 8px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        .info { background: #e7f3ff; padding: 10px; border-radius: 5px; margin: 10px 0; font-size: 0.9em; }
        button { background: #1DB954; color: white; padding: 12px; border: none; border-radius: 5px; width: 100%; font-size: 16px; cursor: pointer; margin: 5px 0; }
        button:hover { background: #169c46; }
        .button-red { background: #dc3545; }
        .button-red:hover { background: #c82333; }
        .button-blue { background: #0066cc; }
        .button-blue:hover { background: #0052a3; }
        .note { color: #666; font-size: 0.85em; margin-top: -10px; margin-bottom: 15px; }
        .current-values { background: #f9f9f9; padding: 10px; border-radius: 5px; margin-bottom: 20px; font-size: 0.9em; border-left: 4px solid #1DB954; }
        .footer { margin-top: 30px; text-align: center; font-size: 0.8em; color: #888; border-top: 1px solid #eee; padding-top: 15px; }
        .footer a { color: #1DB954; text-decoration: none; }
    </style>
</head>
<body>
    <div class="container">
        <h2>🎵 Spotify Player</h2>
        <div class="info">
            <strong>🔗 Permanently available at:</strong><br>
            http://arius.local<br>
            IP: )rawliteral";
  
  html += WiFi.localIP().toString();
  
  html += R"rawliteral(
        </div>
        
        <div class="current-values">
            <strong>📋 Current configuration:</strong><br>
            WiFi: )rawliteral";
  html += wifi_ssid;
  html += R"rawliteral(<br>
            Client ID: )rawliteral";
  html += spotify_client_id;
  html += R"rawliteral(<br>
            Refresh Token: )rawliteral";
  html += (spotify_refresh_token.length() > 0 ? "✓ saved" : "✗ missing");
  html += R"rawliteral(
        </div>
        
        <form action="/save" method="POST">
            <h3>🌐 WiFi</h3>
            <label>Network name (SSID):</label>
            <input type="text" name="wifi_ssid" value=")rawliteral";
  html += wifi_ssid;
  html += R"rawliteral(" required>
            
            <label>WiFi Password:</label>
            <input type="password" name="wifi_password" value=")rawliteral";
  html += wifi_password;
  html += R"rawliteral(" required>
            
            <h3>🎯 Spotify API</h3>
            <div class="note">
                Client ID and Secret can be found at <a href="https://developer.spotify.com/dashboard" target="_blank">Spotify Developer Dashboard</a>
            </div>
            
            <label>Client ID:</label>
            <input type="text" name="client_id" value=")rawliteral";
  html += spotify_client_id;
  html += R"rawliteral(" required>
            
            <label>Client Secret:</label>
            <input type="text" name="client_secret" value=")rawliteral";
  html += spotify_client_secret;
  html += R"rawliteral(" required>
            
            <label>Refresh Token:</label>
            <input type="text" name="refresh_token" value=")rawliteral";
  html += spotify_refresh_token;
  html += R"rawliteral(">
            
            <h3>⚙️ Display Settings</h3>
            
            <label>Title scrolling:</label>
            <select name="scroll_title">
                <option value="1" )rawliteral";
  if (scroll_title) html += "selected";
  html += R"rawliteral(>Enabled</option>
                <option value="0" )rawliteral";
  if (!scroll_title) html += "selected";
  html += R"rawliteral(>Disabled</option>
            </select>
            
            <label>Scroll speed (ms):</label>
            <input type="number" name="scroll_speed" min="50" max="500" value=")rawliteral";
  html += String(scroll_speed);
  html += R"rawliteral(">
            
            <label>Update interval (seconds):</label>
            <input type="number" name="update_interval" min="2" max="60" value=")rawliteral";
  html += String(update_interval);
  html += R"rawliteral(">
            
            <label>Screen brightness (0-255):</label>
            <input type="number" name="brightness" min="0" max="255" value=")rawliteral";
  html += String(brightness);
  html += R"rawliteral(">
            
            <button type="submit">💾 Save settings</button>
        </form>
        
        <form action="/auth" method="GET">
            <button type="submit" class="button-blue">🔑 Spotify Authorization</button>
        </form>
        
        <form action="/restart" method="POST">
            <button type="submit" class="button-blue">🔄 Restart ESP</button>
        </form>
        
        <form action="/reset" method="POST">
            <button type="submit" class="button-red">⚠️ Reset all settings</button>
        </form>
        
        <form action="/apmode" method="POST">
            <button type="submit" class="button-red">📡 Switch to AP mode</button>
        </form>
        
        <!-- FOOTER -->
        <div class="footer">
            Made by <strong>[ELECTRO JSON]</strong> with ❤️ for everyone<br>
            <span style="font-size:0.9em;">© 2026</span>
        </div>
    </div>
</body>
</html>
)rawliteral";

  return html;
}

void handleRoot()
{
  server.send(200, "text/html", getConfigPage());
}

void handleSave()
{
  wifi_ssid = server.arg("wifi_ssid");
  wifi_password = server.arg("wifi_password");
  spotify_client_id = server.arg("client_id");
  spotify_client_secret = server.arg("client_secret");
  spotify_refresh_token = server.arg("refresh_token");
  scroll_title = server.arg("scroll_title") == "1";
  scroll_speed = server.arg("scroll_speed").toInt();
  update_interval = server.arg("update_interval").toInt();
  brightness = server.arg("brightness").toInt();
  
  saveConfig();
  
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
  <style>body{font-family:Arial;padding:20px;background:#f0f0f0;text-align:center}</style></head>
  <body>
      <h2>✅ Saved!</h2>
      <p>Settings have been saved.</p>
      <p><a href="/">← Back to configuration</a></p>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleAuth()
{
  if (spotify_client_id.length() == 0) {
    server.send(200, "text/html", "Save Client ID first!");
    return;
  }
  
  String authUrl = "https://accounts.spotify.com/authorize?";
  authUrl += "client_id=" + spotify_client_id;
  authUrl += "&response_type=code";
  authUrl += "&redirect_uri=http://arius.local/callback";
  authUrl += "&scope=user-read-currently-playing";
  
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
  <style>body{font-family:Arial;padding:20px;background:#f0f0f0;text-align:center}</style></head>
  <body>
      <h2>🔑 Spotify Authorization</h2>
      <p>Click below and log in to Spotify:</p>
      <a href=")rawliteral" + authUrl + R"rawliteral(" style="display:inline-block;background:#1DB954;color:white;padding:12px 20px;border-radius:5px;text-decoration:none;margin:20px 0;">Log in with Spotify</a>
      <p>After authorization, you'll return to configuration with the refresh token saved.</p>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleCallback()
{
  if (server.hasArg("code")) {
    String code = server.arg("code");
    
    WiFiClientSecure client;
    client.setInsecure();
    
    HTTPClient https;
    https.begin(client, "https://accounts.spotify.com/api/token");
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String auth = base64::encode(spotify_client_id + ":" + spotify_client_secret);
    https.addHeader("Authorization", "Basic " + auth);
    
    String body = "grant_type=authorization_code";
    body += "&code=" + code;
    body += "&redirect_uri=http://arius.local/callback";
    
    int httpCode = https.POST(body);
    
    if (httpCode == 200) {
      String response = https.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      
      spotify_refresh_token = doc["refresh_token"].as<String>();
      saveConfig();
      
      String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1">
      <style>body{font-family:Arial;padding:20px;background:#f0f0f0;text-align:center}</style></head>
      <body>
          <h2>✅ Authorization successful!</h2>
          <p>Refresh token has been saved.</p>
          <p><a href="/">← Back to configuration</a></p>
      </body>
      </html>
      )rawliteral";
      
      server.send(200, "text/html", html);
    } else {
      server.send(200, "text/html", "Authorization error: " + String(httpCode));
    }
  }
}

void handleRestart()
{
  server.send(200, "text/html", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleReset()
{
  preferences.begin("spotify", false);
  preferences.clear();
  preferences.end();
  
  server.send(200, "text/html", "Settings reset. Restarting...");
  delay(2000);
  ESP.restart();
}

void handleAPMode()
{
  server.send(200, "text/html", "Switching to AP mode...");
  delay(1000);
  configMode = true;
  startAPMode();
}

void handleNotFound()
{
  server.send(404, "text/plain", "404 - Not found");
}


void startAPMode()
{
  configMode = true;
  
  lcd.fillScreen(lcd.color565(50, 50, 50));
  lcd.setTextColor(lcd.color565(255, 255, 255));
  lcd.setTextSize(2);
  lcd.setCursor(20, 60);
  lcd.print("CONFIGURATION MODE");
  
  lcd.setTextSize(1);
  lcd.setCursor(20, 110);
  lcd.print("1. Connect to WiFi:");
  lcd.setCursor(30, 130);
  lcd.print("Spotify-Config");
  
  lcd.setCursor(20, 160);
  lcd.print("2. Password:");
  lcd.setCursor(30, 180);
  lcd.print("12345678");
  
  lcd.setCursor(20, 210);
  lcd.print("3. Open page:");
  lcd.setCursor(30, 230);
  lcd.print("192.168.4.1");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Spotify-Config", "12345678");
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/auth", handleAuth);
  server.on("/callback", handleCallback);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/apmode", HTTP_POST, handleAPMode);
  server.onNotFound(handleNotFound);
  
  server.begin();
}

void showLogo()
{
  lcd.fillScreen(TFT_BLACK);
  int x = (240 - LOGO_WIDTH) / 2;
  int y = (280 - LOGO_HEIGHT) / 2;
  lcd.pushImage(x, y, LOGO_WIDTH, LOGO_HEIGHT, (uint16_t*)logo_data);
}

void showConnectingScreen()
{
  lcd.fillScreen(lcd.color565(50, 50, 50));
  lcd.setTextColor(lcd.color565(255, 255, 255));
  lcd.setTextSize(2);
  lcd.setCursor(20, 100);
  lcd.print("Connecting to WiFi");
  lcd.setTextSize(1);
  lcd.setCursor(20, 140);
  lcd.print(wifi_ssid);
  lcd.setCursor(20, 180);
  lcd.print("Please wait");
}

void updateConnectingAnimation(int attempts)
{
  lcd.fillRect(150, 180, 60, 20, lcd.color565(50, 50, 50));
  lcd.setTextColor(lcd.color565(255, 255, 255));
  lcd.setTextSize(1);
  int dots = (attempts / 2) % 4;
  String dotsStr = "";
  for (int i = 0; i < dots; i++) dotsStr += ".";
  lcd.setCursor(150, 180);
  lcd.print(dotsStr);
}

void showSpotifyConnecting()
{
  lcd.fillScreen(lcd.color565(50, 50, 50));
  lcd.setTextColor(lcd.color565(255, 255, 255));
  lcd.setTextSize(2);
  lcd.setCursor(20, 120);
  lcd.print("Spotify...");
}

void showConnectedScreen()
{
  lcd.fillScreen(lcd.color565(50, 50, 50));
  lcd.setTextColor(lcd.color565(100, 255, 100));
  lcd.setTextSize(2);
  lcd.setCursor(30, 120);
  lcd.print("Ready!");
  delay(1500);
}


bool refreshAccessToken()
{
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient https;
  https.begin(client, "https://accounts.spotify.com/api/token");
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String auth = base64::encode(spotify_client_id + ":" + spotify_client_secret);
  https.addHeader("Authorization", "Basic " + auth);
  
  String body = "grant_type=refresh_token&refresh_token=" + spotify_refresh_token;
  
  int httpCode = https.POST(body);
  
  if (httpCode == 200) {
    String response = https.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    
    accessToken = doc["access_token"].as<String>();
    tokenExpires = millis() + (doc["expires_in"].as<int>() * 1000) - 60000;
    
    https.end();
    return true;
  }
  
  https.end();
  return false;
}

void drawCoverAsBackground()
{
  if (imageUrl.length() == 0 || !isPlaying) 
  {
    lcd.fillScreen(lcd.color565(50, 50, 50));
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (!https.begin(client, imageUrl)) return;

  int httpCode = https.GET();
  if (httpCode != 200)
  {
    https.end();
    lcd.fillScreen(lcd.color565(50, 50, 50));
    return;
  }

  int len = https.getSize();
  if (len <= 0)
  {
    https.end();
    lcd.fillScreen(lcd.color565(50, 50, 50));
    return;
  }

  uint8_t *buffer = (uint8_t*)ps_malloc(len);
  if (!buffer) buffer = (uint8_t*)malloc(len);
  if (!buffer)
  {
    https.end();
    lcd.fillScreen(lcd.color565(50, 50, 50));
    return;
  }

  WiFiClient *stream = https.getStreamPtr();
  int total = 0;
  int timeout = 0;

  while (https.connected() && total < len && timeout < 5000)
  {
    int available = stream->available();
    if (available)
    {
      int readBytes = stream->readBytes(buffer + total, min(available, len - total));
      total += readBytes;
      timeout = 0;
    }
    else
    {
      delay(10);
      timeout += 10;
    }
  }

  if (total == len)
  {
    TJpgDec.setJpgScale(1);
    TJpgDec.drawJpg(-30, -10, buffer, len);
  }
  else
  {
    lcd.fillScreen(lcd.color565(50, 50, 50));
  }

  free(buffer);
  https.end();
}

bool getCurrentlyPlaying()
{
  if (accessToken.length() == 0) return false;
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient https;
  https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing");
  https.addHeader("Authorization", "Bearer " + accessToken);
  
  int httpCode = https.GET();
  
  if (httpCode == 204) {
    isPlaying = false;
    title = "";
    artist = "";
    imageUrl = "";
    https.end();
    return true;
  }
  
  if (httpCode == 200) {
    String response = https.getString();
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, response);
    
    if (doc.containsKey("item")) {
      String newTitle = doc["item"]["name"].as<String>();
      String newArtist = doc["item"]["artists"][0]["name"].as<String>();
      String newImageUrl = doc["item"]["album"]["images"][1]["url"].as<String>();
      int newProgress = doc["progress_ms"];
      int newDuration = doc["item"]["duration_ms"];
      
      if (!isPlaying || newTitle != title || newArtist != artist || newImageUrl != imageUrl) {
        title = newTitle;
        artist = newArtist;
        imageUrl = newImageUrl;
        progressMs = newProgress;
        durationMs = newDuration;
        isPlaying = true;
        
 
        scrollingBuffer = title + "   " + title + "   ";
        scrollPosition = 0;
        lastScrolledTitle = title;
        
        https.end();
        return true;
      } else {
        progressMs = newProgress;
        durationMs = newDuration;
        https.end();
        return false;
      }
    } else {
      isPlaying = false;
      title = "";
      artist = "";
      imageUrl = "";
      https.end();
      return true;
    }
  }
  
  https.end();
  return false;
}

void drawProgressBar()
{
  int x = 20;
  int y = 250;
  int w = 200;
  int h = 10;

  float percent = (float)progressMs / durationMs;
  if (percent > 1) percent = 1;
  if (percent < 0) percent = 0;

  int fill = percent * w;

  lcd.fillRect(x, y, w, h, lcd.color565(40, 40, 40));
  lcd.fillRect(x, y, fill, h, lcd.color565(100, 255, 100));
}


void drawTextOverlay(bool forceRedraw)
{
  static String lastTitle = "";
  static String lastArtist = "";
  static int lastScrollPos = -1;
  
  if (isPlaying) {

    if (scroll_title && title.length() > 16) {

      if (lastScrolledTitle != title) {
        scrollingBuffer = title + "   " + title + "   ";
        scrollPosition = 0;
        lastScrolledTitle = title;
        forceRedraw = true;
      }
      

      if (millis() - lastScrollTime > scroll_speed) {
        scrollPosition++;
        if (scrollPosition > title.length() + 2) {
          scrollPosition = 0;
        }
        lastScrollTime = millis();
        forceRedraw = true;
      }
      

      if (forceRedraw || scrollPosition != lastScrollPos) {

        lcd.fillRect(15, 205, 210, 20, lcd.color565(40, 40, 40));
        
        lcd.setTextColor(lcd.color565(255, 255, 255));
        lcd.setTextSize(2);
        lcd.setCursor(15, 205);
        String toShow = scrollingBuffer.substring(scrollPosition, scrollPosition + 16);
        lcd.print(toShow);
        
        lastScrollPos = scrollPosition;
      }
    } else {

      if (forceRedraw || lastTitle != title) {
        lcd.fillRect(15, 205, 210, 20, lcd.color565(40, 40, 40));
        lcd.setTextColor(lcd.color565(255, 255, 255));
        lcd.setTextSize(2);
        lcd.setCursor(15, 205);
        String displayTitle = title;
        if (displayTitle.length() > 16) {
          displayTitle = displayTitle.substring(0, 14) + "...";
        }
        lcd.print(displayTitle);
        lastTitle = title;
      }
    }
    

    if (forceRedraw || lastArtist != artist) {
      lcd.fillRect(15, 230, 210, 16, lcd.color565(40, 40, 40));
      lcd.setTextColor(lcd.color565(220, 220, 220));
      lcd.setTextSize(1);
      lcd.setCursor(15, 230);
      String displayArtist = artist;
      if (displayArtist.length() > 30) {
        displayArtist = displayArtist.substring(0, 28) + "...";
      }
      lcd.print(displayArtist);
      lastArtist = artist;
    }
    

    static bool separatorDrawn = false;
    if (!separatorDrawn || forceRedraw) {
      lcd.fillRect(15, 240, 210, 1, lcd.color565(100, 100, 100));
      separatorDrawn = true;
    }
    
  } else {

    static bool noMusicScreenDrawn = false;
    if (!noMusicScreenDrawn || forceRedraw) {
      lcd.fillScreen(lcd.color565(50, 50, 50));
      lcd.setTextColor(lcd.color565(255, 255, 255));
      lcd.setTextSize(2);
      String text = "NO MUSIC";
      int textWidth = text.length() * 12;
      int x = (240 - textWidth) / 2;
      int y = 130;
      lcd.setCursor(x, y);
      lcd.print(text);
      noMusicScreenDrawn = true;
    }
  }
}


void drawScreen(bool forceRedraw)
{
  if (forceRedraw) {
    drawCoverAsBackground();  
  }
  drawTextOverlay(true);      
  if (isPlaying) {
    drawProgressBar();        
  }
}

void updateProgress()
{
  float percent = (float)progressMs / durationMs;
  if (percent > 1) percent = 1;
  if (percent < 0) percent = 0;
  
  int fill = percent * 200;
  

  lcd.fillRect(20, 250, 200, 10, lcd.color565(40, 40, 40));
  lcd.fillRect(20, 250, fill, 10, lcd.color565(100, 255, 100));
}


void startNormalMode()
{
  showLogo();
  delay(2000);
  showConnectingScreen();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  {
    delay(500);
    attempts++;
    updateConnectingAnimation(attempts);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    lcd.setBrightness(brightness);
    
    if (MDNS.begin("arius")) {
      MDNS.addService("http", "tcp", 80);
    }
    
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/auth", handleAuth);
    server.on("/callback", handleCallback);
    server.on("/restart", HTTP_POST, handleRestart);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/apmode", HTTP_POST, handleAPMode);
    server.onNotFound(handleNotFound);
    
    server.begin();
    
    lcd.fillScreen(lcd.color565(50, 50, 50));
    lcd.setTextColor(lcd.color565(255, 255, 255));
    lcd.setTextSize(2);
    lcd.setCursor(20, 80);
    lcd.print("Connected!");
    lcd.setTextSize(1);
    lcd.setCursor(20, 120);
    lcd.print("Configuration:");
    lcd.setCursor(20, 140);
    lcd.print("http://arius.local");
    lcd.setCursor(20, 160);
    lcd.print("IP: " + WiFi.localIP().toString());
    lcd.setCursor(20, 200);
    lcd.print("Hold BOOT button");
    lcd.setCursor(20, 220);
    lcd.print("to enter AP mode");
    delay(4000);
    
    showSpotifyConnecting();
    
    if (refreshAccessToken()) {
      showConnectedScreen();
      

      if (title.length() > 0) {
        scrollingBuffer = title + "   " + title + "   ";
        lastScrolledTitle = title;
      }
    } else {
      lcd.fillScreen(lcd.color565(50, 50, 50));
      lcd.setTextColor(lcd.color565(255, 100, 100));
      lcd.setTextSize(2);
      lcd.setCursor(20, 120);
      lcd.print("Spotify Error!");
      lcd.setCursor(20, 150);
      lcd.print("Wrong token?");
      delay(3000);
    }
  } else {
    lcd.fillScreen(lcd.color565(50, 50, 50));
    lcd.setTextColor(lcd.color565(255, 100, 100));
    lcd.setTextSize(2);
    lcd.setCursor(30, 100);
    lcd.print("WiFi ERROR!");
    lcd.setCursor(20, 140);
    lcd.print("Check password");
    delay(3000);
    configMode = true;
    startAPMode();
  }
}


void setup()
{
  Serial.begin(115200);
  
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);
  
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(180);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextWrap(true);

  TJpgDec.setCallback(tft_output);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setJpgScale(1);

  loadConfig();
  
  if (configMode) {
    startAPMode();
  } else {
    startNormalMode();
  }
  
  isPlaying = false;
  drawScreen(true);
}


unsigned long lastUpdate = 0;
unsigned long lastProgressUpdate = 0;

void loop()
{
  checkButton();
  server.handleClient();
  
  if (configMode) {
    delay(10);
    return;
  }
  
  unsigned long now = millis();
  

  if (now > tokenExpires && tokenExpires > 0) {
    refreshAccessToken();
  }
  

  if (now - lastUpdate > (update_interval * 1000))
  {
    bool changed = getCurrentlyPlaying();
    if (changed) {
      drawScreen(true);  
    }
    lastUpdate = now;
  }
  

  if (now - lastProgressUpdate > 1000 && isPlaying)
  {
    progressMs += 1000;
    if (progressMs > durationMs) progressMs = durationMs;
    updateProgress();  
    lastProgressUpdate = now;
  }
  
 
  drawTextOverlay();
  
  delay(10);
}
