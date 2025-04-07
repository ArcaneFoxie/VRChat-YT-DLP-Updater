# VRChat YT-DLP Updater

A simple utility to update VRChat's internal version of yt-dlp with cookie support, enabling more consistent YouTube playback and avoiding common issues.

## ⚠️ Disclaimer

This application is **NOT** officially approved or endorsed by VRChat. Use of this tool is at your own risk. We are not responsible for any potential actions taken by VRChat team. This tool is provided as-is, without any warranty or guarantee.

## Features

- Automatically updates VRChat's internal yt-dlp to the latest version
- Adds cookie support for YouTube, enabling:
  - More consistent playback
  - Avoids server-side advertisement errors
  - Supports YouTube Premium features (if you have a subscription)
  - Bypasses YouTube bot checks
  - Avoids "advertisements in progress" on Twitch (if you have Twitch Turbo or are subscribed to the channel)
- No need to wait for VRChat to update their yt-dlp version (which can take days)
- Simple one-click operation - just run to install or update

## How to Use

1. Download the latest release
2. Run the application
3. Select your preferred browser when prompted (Firefox is recommended)
4. The application will automatically:
   - Download the latest yt-dlp
   - Configure it with cookie support
   - Install it to VRChat's tools directory
   - Set appropriate permissions

To update in the future, simply run the application again.

## Important Note About Logging In

**Before using the application, make sure you are logged into:**
- YouTube (to bypass restrictions and access Premium features)
- Twitch (if you have Turbo or want to avoid ads on subscribed channels)

The application uses your browser cookies to authenticate with these services. If you're not logged in, you won't get the benefits of cookie support.

## Browser Selection

When you first run the application, you'll be prompted to select a browser. The available options are:
- Firefox (recommended)
- Brave
- Chrome
- Chromium
- Edge
- Opera
- Safari
- Vivaldi
- Whale

**Note:** Firefox is preferred as Chrome-based browsers may fail to work if they are running while loading videos.

## Requirements

- Windows 10 or later
- VRChat installed
- Internet connection (for downloading updates)

## Technical Details

The application:
1. Locates VRChat's tools directory
2. Downloads the latest yt-dlp from GitHub
3. Creates a configuration file with cookie support
4. Sets appropriate file permissions
5. Installs the updated version

## Troubleshooting

If you encounter any issues:
1. Make sure VRChat is not running
2. Try running the application as administrator
3. Check that your browser is properly configured
4. Ensure you have an active internet connection
5. Verify you are logged into YouTube/Twitch in your selected browser

## License

This project is open source and available under the MIT License. 