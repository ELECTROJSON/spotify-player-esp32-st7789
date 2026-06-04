# 🎵 Spotify Player for ESP32-S3 with ST7789 Display

A WiFi-enabled "Now Playing" display for Spotify. Shows album art, track title, artist, and a progress bar on a 240x280 ST7789 display. Configuration is done easily through a web browser in AP mode.

![Demo Image](https://verqstudio.com/pliki/img/1.jpg) <!-- Placeholder for a future demo image -->

## ✨ Features

-   **🎨 Album Art Display:** Fetches and displays high-quality album art as the background.
-   **📝 Track Info:** Shows the track title, artist name, and a live progress bar.
-   **🔄 Scrolling Title:** Automatically scrolls long track titles.
-   **📡 Easy Configuration:** Access a built-in web server to configure WiFi and Spotify API credentials via Access Point (AP) mode.
-   **⚙️ Persistent Settings:** All settings (WiFi, Spotify tokens, display preferences) are saved in the ESP32's non-volatile storage.
-   **🔘 Physical Button:** A button (GPIO 0) to trigger AP mode for reconfiguration.
-   **🔗 mDNS Support:** Access the configuration page locally via `http://arius.local`.

## 🛠️ Hardware Requirements

-   **Microcontroller:** ESP32-S3
-   **Display:** ST7789 (240x280 pixels)
-   **Connectivity:** WiFi 2.4GHz

### 📌 Pin Connections

| ST7789 DISPLAY| ESP32 PIN     |
| ------------- | ------------- |
| GND           | GND           |
| VCC           | 3.3V          |
| SCL           | 8             |
| SDA           | 21            |
| RES           | 4             |
| DC            | 2             |
| CS            | 19            |
| BLK           | 3.3V          |

## 🚀 Getting Started

### 1. Prerequisites

-   **Arduino IDE** with ESP32-S3 (3.3.5) board support installed.
-   Required libraries (install via Arduino Library Manager):
    -   `LovyanGFX` (1.2.7) by lovyan03
    -   `ArduinoJson` (7.4.2) by Benoit Blanchon
    -   `TJpg_Decoder` (1.1.0) by Bodmer
    -   `WebServer` (built-in with ESP32 core)
    -   `Preferences` (built-in with ESP32 core)
    -   `WiFi`, `HTTPClient`, `WiFiClientSecure`, `base64` (built-in with ESP32 core)

### 2. Spotify Developer Setup

To use this project, you need to create a Spotify App.

1.  Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard/) and log in.
2.  Click **"Create an App"**.
3.  Give it a name and description.
4.  Set the **Redirect URI** to: `http://arius.local/callback`
5.  Save your **Client ID** and **Client Secret**.
6.  **Important:** In the App settings, add `http://arius.local/callback` to the **Redirect URIs** list.


### 3. Installing the Code on ESP32-S3
#### Step 1: Configure Arduino IDE
1.  Open Arduino IDE.
2.  Go to **Tools → Board → Boards Manager**.
3.  Search for `ESP32` and install **ESP32 by Espressif Systems** (version 3.3.0).
4: Install Required Libraries
Open **Tools → Manage Libraries** and install:

| Library | Version | Author |
|---------|---------|--------|
| LovyanGFX | 1.2.7 | lovyan03 |
| ArduinoJson | 7.4.2 | Benoit Blanchon |
| TJpg_Decoder | 1.1.0 | Bodmer |

5. Before uploading, you need to select the correct board and settings for your ESP32-S3.

1.  Go to **Tools → Board → ESP32 Arduino → ESP32S3 Dev Module**
2.  Set the following configuration options:

| Setting | Value | Notes |
|---------|-------|-------|
| **USB Mode** | `USB-OTG` | Required for serial communication over USB |
| **USB CDC On Boot** | `Enabled` | Enables virtual serial port |
| **CPU Frequency** | `240MHz (WiFi)` | Optimal performance for WiFi + display |
| **Flash Size** | `16MB (128Mb)` | Adjust based on your board (4MB, 8MB, or 16MB) |
| **Partition Scheme** | `Huge APP (3MB No OTA/1MB SPIFFS` | Enough space for code and settings |


#### Step 2: Connect ESP32-S3 to Computer

1.  Connect the ESP32-S3 to your computer using a **USB cable** (most S3 boards use USB-C).
2.  **Select the port in Arduino IDE:**
    -   Go to **Tools → Port** and select the correct COM port (e.g., `COM3`, etc.)

#### Step 3: Upload the Code

Now you're ready to upload the sketch to your ESP32-S3.

1.  **Open the sketch:**
    -   If you haven't already, copy the full Arduino code into a new sketch.
    -   Save it as `Spotify_Player` (File → Save As...).

2.  **Verify the code:**
    -   Click the **✓ Verify** button (checkmark) in the top-left corner.
    -   Wait for compilation to complete.
    -   If there are errors, check that all libraries are installed correctly.

3.  **Upload the code:**
    -   Click the **→ Upload** button (right arrow).
    -   Watch the status messages at the bottom of the Arduino IDE.

4.  **Handle the bootloader mode (if needed):**
   
**Do this:**
1.  Press and **hold** the **BOOT** button on your ESP32-S3 board.
2.  Press and **release** the **RESET** (EN) button.
3.  **Release** the BOOT button.
4.  Upload should now start.

> **📌 Note:** Many modern S3 boards have auto-download circuitry and don't require this step.

5.  **Wait for upload to complete:**
Writing at 0x00010000... (100%)

✅ **Success!** The code is now on your ESP32-S3.

### 3. First-Time Configuration (AP Mode)

1.  Open the Serial Monitor (115200 baud). The device will start in Configuration Mode if no WiFi settings are found.
2.  On the display, you will see "CONFIGURATION MODE".
3.  Use your phone or computer to connect to the new WiFi network named **`Spotify-Config`** with the password **`12345678`**.
4.  Open a web browser and go to `192.168.4.1`.
5.  You will see the configuration page. Fill in the following:
    -   **🌐 WiFi:** Your home/office WiFi **SSID** and **Password**.
    -   **🎯 Spotify API:** The **Client ID** and **Client Secret** you got from the Spotify Developer Dashboard.
6.  Click **"Save settings"**.
7.  The page will refresh. Now, click the **"🔑 Spotify Authorization"** button.
8.  You will be redirected to a Spotify login page. Log in to the Spotify account you want the player to track.
9. Click **"Agree"** to authorize the app. You will be redirected back to the configuration page with a success message.
10. The ESP32 will now automatically restart and try to connect to your WiFi and Spotify.

### 4. Normal Operation

Once configured, the device will:
1.  Display a boot logo.
2.  Connect to your WiFi.
3.  Show the local IP address and `http://arius.local` address.
4.  Connect to Spotify and start displaying the currently playing track.
5.  The progress bar will fill in real-time.
6.  **To enter Configuration Mode again**, press and hold the button (GPIO 0) for 3 seconds.

## 🕹️ Web Interface & Configuration

The built-in web server (accessible at `http://arius.local` or the device's IP address) allows you to manage all settings:

-   **WiFi:** Change your network credentials.
-   **Spotify API:** Update Client ID/Secret or re-authorize.
-   **Display Settings:**
    -   `Title scrolling:` Enable/disable scrolling for long titles.
    -   `Scroll speed (ms):` Adjust the animation speed.
    -   `Update interval (s):` How often to fetch new track data from Spotify (default 5s).
-   **System Controls:**
    -   `Restart ESP:` Reboot the device.
    -   `Reset all settings:` Clear all saved WiFi and API credentials.
    -   `Switch to AP mode:` Force the device back into configuration mode.

## 💬 Need Help?

If you encounter any issues not covered in the troubleshooting section, or if you just want to share your build, I'm here to help!

### Join the Community

[![Discord](https://img.shields.io/badge/Discord-Join-%235865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/gNCctNDwNR)

**Get support, share ideas, and showcase your project:**

-   🐛 Report bugs and get troubleshooting help
-   💡 Suggest new features or improvements
-   📸 Share photos of your finished build
-   🔧 Ask questions about modifications and customizations
-   🤝 Connect with other makers and Spotify API enthusiasts

**When asking for help, please include:**
-   Your ESP32-S3 board model
-   Display version (ST7789, 240x280)
-   Any error messages from the Serial Monitor
-   Photos of your wiring (if applicable)
-   Steps you've already tried

> **🎯 Quick tip:** Before reaching out, double-check your wiring connections and make sure you've completed the Spotify authorization step. Most issues are solved by re-authorizing the Spotify token!

---

## 📝 Final Notes

-   **This project is open source** - Feel free to modify, improve, and share it!
-   **Created with ❤️ by JSON<3** - For the ESP32 and music lovers community
-   **License:** Open Source - Free to use, modify, and share
-   **Year:** 2026

---

## 🙏 Thank You!

Thank you for building this project! I hope it brings as much joy to your desk or workshop as it brought me while creating it.

---

*Happy coding and happy listening!*

**— JSON<3**



