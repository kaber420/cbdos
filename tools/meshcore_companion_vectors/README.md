# Vectores binarios MeshCore Fase 1

Genera frames `Radio→App` (`>` + len u16 LE + payload) para probar
`MeshCoreClient::processByte()` sin hardware.

- `CONTACT_START [count]` / `CONTACT [body 147B]` / `CONTACT_END [lastmod]`
- `ADVERT / NEW_ADVERT [body 147B]`
- `ACK [tag u32 LE]` / `MSG_SENT [flood, tag, timeout]` / `CURRENT_TIME`
- `CHANNEL_MSG_V3` / `CONTACT_MSG_V3` con SNR

Uso:

```bash
python3 tools/meshcore_companion_vectors/gen_vectors.py --out /tmp/meshvec
```

Cada vector es un `.bin` con el framing `>` completo listo para inyectar
byte a byte en `processByte()`.
