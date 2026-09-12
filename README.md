# ESP32-C3 Image Vault

A beginner-friendly wireless image storage project using an **ESP32-C3 Super Mini** and **W25Q128 16MB SPI flash memory**.

Upload an image from your phone or laptop over Wi-Fi, store it in external flash, and view it again through a browser — all without any cloud server.

![ESP32-C3 Image Vault](images/project-banner.png)

## Features

- ESP32-C3 Super Mini
- W25Q128 16MB external SPI flash
- Wi-Fi Access Point mode
- Browser-based image upload
- Image preview before upload
- Upload progress
- Store image in W25Q128 flash
- Read and display stored image
- Delete stored image
- No internet or cloud required
- Arduino IDE compatible

## How It Works

```text
Phone / Laptop
      |
    Wi-Fi
      |
  ESP32-C3
      |
  Web Server
      |
     SPI
      |
  W25Q128
External Flash
```

The ESP32-C3 creates its own Wi-Fi network. After connecting to it, open the local web page and upload an image.

The ESP32 receives the image and stores it in the W25Q128 external flash memory through SPI communication.

## Hardware Required

| Component | Quantity |
|---|---:|
| ESP32-C3 Super Mini | 1 |
| W25Q128 SPI Flash Module | 1 |
| Breadboard | 1 |
| Jumper Wires | Several |
| USB-C Cable | 1 |
| Phone or Laptop | 1 |

## Wiring

| W25Q128 Pin | ESP32-C3 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CLK / SCK | GPIO 4 |
| DO / MISO | GPIO 5 |
| DI / MOSI | GPIO 6 |
| CS | GPIO 7 |

> **Important:** Power the W25Q128 with **3.3V only**.

![Wiring Connection](images/wiring-connection.png)

## Tested Flash

The W25Q128 was successfully detected using its JEDEC ID:

```text
Manufacturer : 0xEF
Memory Type  : 0x40
Capacity     : 0x18
Full ID      : EF 40 18
```

The following operations were tested successfully:

```text
Erase
Write
Read
Compare
```

## Wi-Fi Details

The ESP32 runs in Access Point mode.

```text
SSID: ESP32-VAULT
Password: 12345678
```

After connecting, open:

```text
http://192.168.4.1
```

## Arduino IDE Setup

This project is made for **Arduino IDE**.

Recommended board selection:

```text
ESP32C3 Dev Module
```

Serial Monitor baud rate:

```text
115200
```

## Usage

1. Connect the ESP32-C3 and W25Q128 using the wiring table above.
2. Open `ESP32_C3_Image_Vault.ino` in Arduino IDE.
3. Select the ESP32-C3 board and correct serial port.
4. Upload the sketch.
5. Open Serial Monitor at `115200`.
6. Confirm that the flash ID is detected.
7. Connect your phone or laptop to `ESP32-VAULT`.
8. Open `http://192.168.4.1`.
9. Select an image.
10. Preview the image.
11. Click **Upload to W25Q128**.
12. Wait for the upload to complete.
13. View the stored image directly from external flash.

## Web Interface

The browser interface allows you to:

- Choose an image
- Preview the selected image
- Upload it to W25Q128
- View upload progress
- Read the stored image
- Refresh the stored image
- Delete the image

![Web Interface](images/web-interface.png)

## Project Structure

```text
ESP32-C3-Image-Vault/
|
├── ESP32_C3_Image_Vault.ino
├── README.md
├── LICENSE
|
├── web/
|   └── ESP32_C3_Image_Vault_Web_UI.html
|
└── images/
    ├── project-banner.png
    ├── wiring-connection.png
    ├── prototype-1.jpg
    ├── prototype-2.jpg
    └── web-interface.png
```

## Flash Memory

The W25Q128 provides:

```text
128 Mbit = 16 MB
```

Typical memory organization:

```text
Sector Size : 4 KB
Page Size   : 256 bytes
```

In this project, image data is stored directly in external flash memory.

## SPI Pins Used

```cpp
#define FLASH_SCK   4
#define FLASH_MISO  5
#define FLASH_MOSI  6
#define FLASH_CS    7
```

## Applications

This project can be expanded into:

- Wireless image gallery
- Digital signage
- Smart display
- Offline photo viewer
- Product image viewer
- Local media storage
- ESP32 web gallery
- Portable image vault

## Future Improvements

Planned improvements include:

- Multiple image storage
- Image gallery
- Drag-and-drop upload
- Rename images
- Download images
- Search and sorting
- Storage percentage indicator
- Fullscreen viewing
- Admin and user pages
- Captive portal
- Automatic image refresh

## Prototype

![Prototype](images/prototype-1.jpg)

![Prototype](images/prototype-2.jpg)

## Built With

- ESP32-C3 Super Mini
- W25Q128 SPI Flash
- Arduino IDE
- HTML
- CSS
- JavaScript
- ESP32 Wi-Fi
- SPI

## License

This project is open source and can be used for learning, prototyping, and personal projects.

## Author

Built as a beginner-friendly ESP32 hardware and web interface project.

---

If you like this project, give the repository a ⭐ and feel free to build your own version.
