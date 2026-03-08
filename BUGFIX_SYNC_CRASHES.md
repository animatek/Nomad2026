# Correcciones Críticas: Crashes y StorePatch

## Problemas Encontrados y Solucionados

### 1. ❌ CRASH al Cargar Patch del Sintetizador

**Síntoma:** El editor se cierra (crash) cuando:
- Se hace "Device → Request Patch from Synth"
- Se cambia de patch en el sintetizador

**Causa Raíz:**
El `PatchSynchronizer` mantiene una referencia al objeto `Patch` anterior. Cuando se carga un nuevo patch:
1. El código reemplazaba `currentPatch` con el nuevo patch
2. El patch anterior se destruía
3. El `PatchSynchronizer` todavía tenía una referencia al patch destruido
4. Cuando el synchronizer intentaba acceder al patch → **CRASH**

**Solución:**
Destruir el `PatchSynchronizer` ANTES de reemplazar el patch:

```cpp
// ANTES (INCORRECTO - causaba crash):
currentPatch = std::move(newPatch);
patchSynchronizer = std::make_unique<PatchSynchronizer>(*currentPatch, ...);

// DESPUÉS (CORRECTO):
patchSynchronizer.reset();  // Destruir PRIMERO
currentPatch = std::move(newPatch);  // Luego reemplazar
patchSynchronizer = std::make_unique<PatchSynchronizer>(*currentPatch, ...);
```

**Archivos Modificados:**
- `MainComponent.cpp`:
  - `setPatchDataCallback()` - Cuando se recibe patch del sintetizador
  - `loadPatchFromFile()` - Cuando se carga patch de archivo
  - `newPatch()` - Cuando se crea nuevo patch

---

### 2. ❌ StorePatch No Guardaba Correctamente

**Síntoma:**
- Al hacer "Device → Send Patch to Synth", aparece confirmación de guardado
- Pero al recargar el patch desde el sintetizador, los cambios no persisten

**Causa Raíz:**
El formato del mensaje `StorePatchMessage` estaba **INCORRECTO**.

**Formato Implementado Inicialmente (INCORRECTO):**
```
Byte 0: slot:2 section:1 0:5   (solo 2 bits para slot)
Byte 1: position:7 0:1
```

**Formato Correcto según PDL2 spec:**
```
Byte 0: 0:1 slot:7     (7 bits completos para slot)
Byte 1: 0:1 section:7  (7 bits para section)
Byte 2: 0:1 position:7 (7 bits para position)
```

**Solución:**
Reimplementar `StorePatchMessage::toSysEx()` con el formato correcto:

```cpp
// Payload (3 bytes) per PDL2 spec:
// 0:1 slot:7 0:1 section:7 0:1 position:7

int byte0 = (slot_ & 0x7F);      // Byte 0: slot
int byte1 = (section_ & 0x7F);   // Byte 1: section
int byte2 = (position_ & 0x7F);  // Byte 2: position

msg.push_back(byte0);
msg.push_back(byte1);
msg.push_back(byte2);
```

**Archivos Modificados:**
- `source/protocol/StorePatchMessage.cpp` - Formato corregido
- `source/protocol/StorePatchMessage.h` - Comentarios actualizados

---

## Verificación y Debugging

### Logs de Debug Mejorados

Ahora el código genera logs más detallados para facilitar debugging:

**Cuando se carga un patch:**
```
Patch synchronizer enabled after patch load
```

**Cuando se cambia de patch:**
```
Patch synchronizer disabled on disconnect
Patch synchronizer enabled after patch load
```

**Cuando se guarda a sintetizador:**
```
Sent StorePatch: slot=0 section=0 position=0
StorePatch SysEx: F0 33 06 17 41 0B 00 00 00 [checksum] F7
```

### Cómo Probar las Correcciones

#### Test 1: Cargar Patch del Sintetizador (sin crash)
1. Conectar al sintetizador
2. **Device → Request Patch from Synth**
3. **✅ Esperado:** El patch se carga sin crash
4. Verificar en debug log: `Patch synchronizer enabled after patch load`

#### Test 2: Cambiar Patch en el Sintetizador (sin crash)
1. Con editor conectado y patch cargado
2. En el sintetizador físico, cambiar a otro patch
3. **✅ Esperado:** El editor NO se cierra
4. Verificar en debug log que el synchronizer se destruye/recrea correctamente

#### Test 3: Guardar Patch al Sintetizador (persistencia real)
1. Conectar y cargar patch
2. Hacer un cambio visible (añadir cable, mover módulo)
3. **Device → Send Patch to Synth**
4. Verificar en debug log:
   ```
   Sent StorePatch: slot=0 section=0 position=0
   StorePatch SysEx: F0 33 06 17 41 0B 00 00 00 XX F7
   ```
5. **Device → Request Patch from Synth** (recargar)
6. **✅ Esperado:** Los cambios PERSISTEN

#### Test 4: Edición en Tiempo Real
1. Conectar y cargar patch
2. Añadir cable → verificar log: `Sent NewCable: section=...`
3. Borrar cable → verificar log: `Sent DeleteCable: section=...`
4. Mover módulo → verificar log: `Sent MoveModule: section=... pos=(...)`
5. **Device → Send Patch to Synth**
6. Recargar patch
7. **✅ Esperado:** Todos los cambios persisten

---

## Notas Importantes

### ⚠️ Parámetro `section` en StorePatch

El significado exacto del parámetro `section` **NO está completamente documentado**:
- Valor actual: `section=0`
- Mejor hipótesis: `0` = guardar ambas áreas (poly + common)
- Valor alternativo: `1` = solo poly, `2` = solo common (?)

**Si StorePatch sigue sin funcionar**, probar cambiar el valor de `section`:
```cpp
// En MainComponent::savePatchToSynth()
int section = 0;  // Probar con 1 o 2 si 0 no funciona
```

### ⚠️ Parámetro `position` sobrescribe

Actualmente `position=0` está hardcodeado, lo que significa:
- **Siempre guarda en la posición 0 del banco**
- **Esto SOBRESCRIBE el patch que esté en esa posición**

**TODO:** Agregar un diálogo para que el usuario elija:
- Bank (0-99 para user banks)
- Position dentro del bank
- Slot destino (puede ser diferente al slot actual)

---

## Resumen de Cambios

### Archivos Modificados

**Corrección de Crashes:**
- `source/MainComponent.cpp`:
  - Línea ~86: Reset synchronizer antes de reemplazar patch (patch from synth)
  - Línea ~245: Reset synchronizer antes de new patch
  - Línea ~315: Reset synchronizer antes de load from file
  - Línea ~350-385: Mejores logs de debug para StorePatch

**Corrección de StorePatch:**
- `source/protocol/StorePatchMessage.cpp`:
  - `toSysEx()`: Formato corregido a 3 bytes (slot, section, position)
- `source/protocol/StorePatchMessage.h`:
  - Comentarios actualizados con formato PDL2 correcto

### Commit

```bash
git add source/MainComponent.cpp source/protocol/StorePatchMessage.*
git commit -m "Fix critical crashes and StorePatch message format

- Fix crash when loading patch from synth (destroy synchronizer before patch replacement)
- Fix crash when changing patch on synth hardware
- Fix crash when creating new patch
- Fix StorePatch message format (was using wrong bit packing)
- Add detailed debug logging for StorePatch operations

The synchronizer was holding a reference to the old Patch object
when it was being destroyed, causing segfaults. Now we properly
reset the synchronizer before replacing the patch.

The StorePatch message was using incorrect format (2-byte payload
with bit-packed slot/section). Corrected to PDL2 spec: 3 bytes
with slot:7, section:7, position:7.
"
```

---

## Próximos Pasos

Si todavía hay problemas:

1. **Capturar logs completos:**
   ```bash
   ./build/Nomad2026_artefacts/Release/Nomad2026 2>&1 | tee nomad.log
   ```
   Enviar el archivo `nomad.log` para análisis

2. **Comparar con Nomad original:**
   Usar un monitor MIDI para ver qué mensajes envía el Nomad Java cuando guarda:
   ```bash
   aseqdump -p "Nord Modular"  # En Linux
   ```

3. **Verificar respuesta del sintetizador:**
   El sintetizador debería enviar un ACK después de StorePatch si la operación fue exitosa

4. **Probar diferentes valores de section:**
   Cambiar `int section = 0;` a `1` o `2` en `savePatchToSynth()`
