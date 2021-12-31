# Time-based One-time Password Smart Safe

[![TOTP Safe](http://img.youtube.com/vi/4Zr3-eivLDk/0.jpg)](http://www.youtube.com/watch?v=4Zr3-eivLDk "TOTP Safe")

This is the code repository for my TOTP Safe project. For a full description of the project, please see https://www.instructables.com/Time-Based-One-Time-Password-TOTP-Smart-Safe/. 

## Changing the seed
The code in the repository defines a key (seed) for the TOTP. The key is the string "My safe" in hex. You can use any online Text to Hex converter, such as: [Text to Hex Converter - Online Toolz](https://www.online-toolz.com/tools/text-hex-convertor.php).

<code>uint8_t hmacKey[] = {0x4D, 0x79, 0x20, 0x73, 0x61, 0x66, 0x65};</code>

With this key in place, our TOTP implementation will be able to calculate a new 6 digit key every 30 seconds.

Next you'll need to create an entry in your favorite Authenticator app (Microsoft's Authenticator, Google's). Both apps will require the <b>BASE32 representation of the key</b> ("My safe" in my case). You can use any online Base32 Encoder, for example: [Base32 Encode Online (emn178.github.io)](https://emn178.github.io/online-tools/base32_encode.html).

The base32 encoded string (e.g. "My safe" -> "JV4SA43BMZSQ") is the Key to be used for the Authenticator entry. You can enter this manually in your Authenticator app, or you can use a QR. A simple online tool for creating QR codes that Authenticator apps understand is [Generate QR Codes for Google Authenticator (hersam.com)](https://dan.hersam.com/tools/gen-qr-code.php).

Alternatively, just for testing, you can use [TOTP Generator (danhersam.com)](https://totp.danhersam.com/?key=JV4SA43BMZSQ). You can manually enter the key (again, in base32 encoded string), and it will prompt you for the 6 digit one-time password, which the safe will accept in the 30 second time window.
