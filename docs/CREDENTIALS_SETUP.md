# LoRaWAN Credentials Setup

## Security Notice

⚠️ **IMPORTANT**: LoRaWAN credentials are device-specific secrets that should **NEVER** be committed to version control or shared publicly. The `src/credentials.h` file is gitignored to prevent accidental exposure.

## Initial Setup

### 1. Create Your Credentials File

Copy the template to create your credentials file:

```bash
cp src/credentials.h.template src/credentials.h
```

### 2. Get Your Device Keys from TTN

1. Log into [The Things Network Console](https://console.cloud.thethings.network/)
2. Navigate to: **Applications → Your App → End devices → Your Device**
3. Copy the following values (use **MSB format**):
   - **DevEUI** (8 bytes)
   - **AppEUI** / **JoinEUI** (8 bytes, often all zeros for TTN v3)
   - **AppKey** (16 bytes)

### 3. Update credentials.h

Edit `src/credentials.h` and paste your keys:

```cpp
// Example - replace with your actual keys from TTN console
uint8_t nodeDeviceEUI[8] = {0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x07, 0x5A, 0x89};
uint8_t nodeAppEUI[8]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t nodeAppKey[16]   = {0x77, 0x28, 0xEE, 0x05, 0x47, 0xEC, 0xBB, 0x32,
                             0x48, 0x23, 0x4E, 0x6D, 0xC2, 0x8A, 0xED, 0x8D};
```

### 4. Verify .gitignore

Ensure `src/credentials.h` is in `.gitignore`:

```bash
grep "credentials.h" .gitignore
```

Should show:
```
src/credentials.h
```

## Multiple Devices

If you're deploying multiple sensors:

1. Each device needs **unique credentials** (especially DevEUI and AppKey)
2. Register each device separately in TTN console
3. Create a separate `credentials.h` for each device
4. Consider naming them `credentials_device1.h`, `credentials_device2.h`, etc.
5. Update `lorawan.cpp` to include the correct file when flashing

## Security Best Practices

✅ **DO:**
- Keep `credentials.h` gitignored
- Use unique keys for each device
- Regenerate AppKey if compromised
- Store backup of keys in password manager

❌ **DON'T:**
- Commit credentials.h to git
- Share keys in public forums/issues
- Reuse keys across devices
- Screenshot keys and share publicly

## Troubleshooting

### Build fails: "credentials.h: No such file"

You need to create the credentials file:
```bash
cp src/credentials.h.template src/credentials.h
# Then edit with your actual keys
```

### Device won't join network

1. Check keys are correct (MSB format)
2. Verify device is registered in TTN
3. Check LoRaWAN gateway coverage
4. Watch Serial output: `LoRaWAN: joined network` on success

### Keys got committed to git

1. **Immediately regenerate AppKey** in TTN console
2. Update your local `credentials.h`
3. Remove from git history:
   ```bash
   git filter-branch --force --index-filter \
     "git rm --cached --ignore-unmatch src/credentials.h" \
     --prune-empty --tag-name-filter cat -- --all
   ```
4. Force push (⚠️ dangerous if others are using repo)

## Alternative: BLE Configuration (Future)

For production, consider implementing BLE-based credential provisioning:
- Store credentials in nRF52 flash memory
- Configure via BLE UART on first boot
- No need for recompilation per device
- More secure for deployment

This would require implementing:
- Flash storage using nRF52 SDK
- BLE command interface for setting keys
- Secure storage with encryption
