# requirementGenerator

C kaynak kodundan (**`.h`**, **`.c`**, **`.s`**) IBM DOORS gereksinim modülleri üretmek için Python tabanlı bir araç.

Parser, kaynak dosyaları okuyup pipe-delimited `.txt` dosyaları oluşturur. Bu dosyalar DXL scriptleri aracılığıyla DOORS'a aktarılır.

---

## Genel Akış

```
.h / .c / .s  →  [Python Parser]  →  txt dosyaları  →  [DXL Script]  →  DOORS Modülü
```

| Kaynak | Üretilen Txt | DOORS Modül Türü |
|--------|-------------|-----------------|
| `foo.h` | `output/foo_DD_HEADER.txt` | DD_HEADER |
| `foo.c` | `output/foo_SRS_LLR.txt` | SRS_LLR |
| `foo.s` | `output/foo_SRS_LLR.txt` | SRS_LLR |

---

## Kurulum

Python 3.8+ gereklidir. Ek bağımlılık yoktur.

```bash
git clone <repo-url>
cd requirementGenerator
```

---

## Kullanım

```bash
# Bulunduğun dizindeki tüm .h / .c / .s dosyalarını işle
python main.py .

# Belirli bir dizini işle
python main.py path/to/source/

# Tek dosya
python main.py module.h
python main.py module.c

# Özel çıktı klasörü (varsayılan: output/)
python main.py -o out/ path/to/source/
```

Üretilen dosyalar `output/` klasörüne yazılır.

---

## Aşama 1 — Parser (Python)

`reqgen/` paketi kaynak kodu ayrıştırır:

| Modül | Görevi |
|-------|--------|
| `header_parser.py` | `.h` → define, typedef, enum, struct, union, funcptr, inline |
| `source_parser.py` | `.c` → fonksiyonlar + inline `__asm__` blokları |
| `asm_parser.py` | `.s` → standalone assembly fonksiyonları + data sembolleri |
| `doxygen_parser.py` | `@brief` / `@param` / `@return` ve düz blok comment parse |
| `size_calculator.py` | C tipi → byte boyutu (x86-64 / LP64) |
| `writer.py` | Yapısal dict → pipe-delimited txt |

### DD_HEADER Txt Formatı

```
SECTION|Structures
STRUCT|Sensor|struct Sensor|38|Sensor is a structure type|...
STRUCT_FIELD|Sensor|id|uint8_t|1|0
STRUCT_FIELD|Sensor|name|char[32]|32|0
STRUCT_FIELD|Sensor|value|f32|4|0
STRUCT_FIELD|Sensor|is_active|bool|1|0
```

### SRS_LLR Txt Formatı

```
FUNC_HEADING|sensor_read|Read Sensor Data|false
FUNC_REQ|sensor_read|The sensor_read() function shall read sensor data.|true
FUNC_PARAM|sensor_read|The sensor_read() function shall accept parameter sensor: ...|true
FUNC_RETURN|sensor_read|The sensor_read() function shall return a value of type bool.|true

ASM_HEADING|asm_demo|Inline assembly (GCC / MinGW — x86-64)|false
ASM_REQ|asm_demo|The asm_demo() function shall execute a no-operation (NOP) instruction.|true
ASM_REQ|asm_demo|The asm_demo() function shall retrieve CPU identification information via the CPUID instruction.|true
```

---

## Aşama 2 — DD_HEADER DOORS Modülü (DXL)

1. DOORS'ta hedef `DD_HEADER` modülünü aç.
2. `dxl/dd_header_import.dxl` dosyasını aç.
3. En üstteki `FILE_PATH` sabitini üretilen txt dosyasının yoluyla güncelle.
4. **Tools → Edit DXL...** ile yapıştır ve çalıştır  
   *veya* **Tools → Run DXL File...** ile doğrudan çalıştır.

Oluşturulan DOORS attribute'ları:

| Attribute | Açıklama |
|-----------|----------|
| `Object Heading` | Bölüm ve tip başlıkları |
| `Object Text` | Tanım metni |
| `Data Type` | C veri tipi |
| `Size (Bytes)` | Byte cinsinden boyut |
| `Description` | Detaylı açıklama |

---

## Aşama 3 — SRS_LLR DOORS Modülü (DXL)

1. DOORS'ta hedef `SRS_LLR` modülünü aç.
2. `dxl/srs_llr_import.dxl` dosyasını aç.
3. `FILE_PATH` sabitini güncelle ve çalıştır.

Oluşturulan DOORS attribute'ları:

| Attribute | Açıklama |
|-----------|----------|
| `Object Heading` | Fonksiyon başlığı |
| `Object Text` | Gereksinim cümlesi ("The X() function shall...") |
| `isReq` | `true` = gereksinim, `false` = başlık |

---

## Doxygen Desteği

Parser, her C construct'ından önce gelen yorumu otomatik okur:

```c
/**
 * @brief Read sensor data into the provided sensor structure.
 * @param sensor Pointer to the sensor structure to populate.
 * @return true if successful, false if sensor pointer is NULL.
 */
bool sensor_read(Sensor *sensor);
```

- `@brief` → fonksiyon başlığı ve ana gereksinim cümlesi
- `@param` → her parametre için ayrı `FUNC_PARAM` satırı
- `@return` → `FUNC_RETURN` satırı

Doxygen yoksa plain `/* */` yorumu veya fonksiyon adı kullanılır.

---

## Proje Yapısı

```
requirementGenerator/
├── main.py                    ← giriş noktası
├── reqgen/
│   ├── header_parser.py
│   ├── source_parser.py
│   ├── asm_parser.py
│   ├── doxygen_parser.py
│   ├── size_calculator.py
│   └── writer.py
├── output/                    ← üretilen txt dosyaları
├── dxl/
│   ├── dd_header_import.dxl
│   └── srs_llr_import.dxl
├── sample.h                   ← referans / test girdisi
├── sample.c
└── sample_asm.s
```

`sample.*` dosyaları, parser'ı test etmek amacıyla her C11 ve x86-64 construct'ını kapsayan referans kaynaklardır.
