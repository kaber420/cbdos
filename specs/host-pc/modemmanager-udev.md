# ModemManager vs placas ESP32 por USB (interferencia y cómo corregirla)

> Afecta poco, pero está ahí: fallos aleatorios de "puerto ocupado", timeouts de
> esptool y basura `AT` llegando al CLI serial de CBDos (`[SERIAL_CLI_ERR]`).

## Qué pasa

Ubuntu trae **ModemManager** (parte de NetworkManager) para módems USB de internet
móvil. Cada vez que aparece un `/dev/ttyACM*` o `/dev/ttyUSB*`, lo abre y le manda
comandos **AT** (`AT`, `AT+CGMI`, …) para ver si es un módem celular.

Las placas ESP32 (USB nativo `303a:1001`, puentes CP210x `10c4:ea60`,
CH340 `1a86:7523`) **no** son módems, pero igual las manosea unos segundos:

- roba el puerto justo cuando lo necesitas (flasheo, WebSerial, esptool);
- sus ATs llegan al `SERIAL_CLI` de CBDos y Lua intenta ejecutarlos (errores fantasma).

Esto **NO** afecta a los puentes C3/ESP-NOW como red: esos hablan serial CDC con
protocolo propio (`0xAA 0x55`, `tlvgl_gateway`, esptool) y nada de ese tráfico
pasa por ModemManager. La corrección solo le dice "no toques estos VID:PID";
el `/dev/tty*` sigue disponible para todo lo demás. Un módem LTE real (Quectel,
SIMCom, …) usa otros VID:PID y seguiría funcionando como uplink.

## Corrección (una vez por PC)

Crear `/etc/udev/rules.d/99-espressif.rules`:

```
# Ignorar placas ESP32 y puentes serie comunes en ModemManager
ATTRS{idVendor}=="303a", ENV{ID_MM_DEVICE_IGNORE}="1"
ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", ENV{ID_MM_DEVICE_IGNORE}="1"
ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", ENV{ID_MM_DEVICE_IGNORE}="1"
```

Aplicar:

```bash
sudo tee /etc/udev/rules.d/99-espressif.rules > /dev/null <<'EOF'
ATTRS{idVendor}=="303a", ENV{ID_MM_DEVICE_IGNORE}="1"
ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", ENV{ID_MM_DEVICE_IGNORE}="1"
ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", ENV{ID_MM_DEVICE_IGNORE}="1"
EOF
sudo udevadm control --reload-rules
# Reconecta la placa (o reinicia ModemManager):
sudo systemctl restart ModemManager
```

Verificar (con la placa conectada, debe salir vacía):

```bash
mmcli -L
```

## Nota: `brltty`

Otro clásico de Ubuntu que secuestra puertos serie (lector de pantalla para
braille). Si `systemctl is-active brltty` dice `active` y te roba la placa:

```bash
sudo systemctl disable --now brltty
```
