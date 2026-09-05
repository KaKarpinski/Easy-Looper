# Easy Looper

Prosty live looper AU/VST3 na macOS (JUCE), pod gitarę w FL Studio i kontroler M-VAVE Chocolate.

MVP: Record, Play/Stop, Overdub, Undo, Clear, MIDI Learn. Bez syncu do BPM.

## Co musisz mieć na Macu

1. **Narzędzia kompilacji Apple** — Command Line Tools wystarczą (`clang` jest OK). Pełne Xcode z App Store też działa.
2. **CMake 3.22+**:

```bash
brew install cmake
```

3. **Git** — pierwsza konfiguracja CMake ściągnie JUCE, jeśli nie ma folderu `JUCE/`.

## Zbudowanie pluginu

W katalogu projektu (CMake 3.22+):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/EasyLooperTests
bash scripts/install-macos.sh
```

Skrypt kopiuje pluginy do:

- `~/Library/Audio/Plug-Ins/Components/Easy Looper.component` (AU)
- `~/Library/Audio/Plug-Ins/VST3/Easy Looper.vst3` (VST3)

Gotowe binaria po buildzie leżą też w `build/EasyLooper_artefacts/Release/`.

## Podłączenie w FL Studio (Mac)

### 1. Skan pluginów

1. Otwórz **FL Studio 2024** (albo 21).
2. **Options → Manage plugins**.
3. Upewnij się, że skanuje VST3 i AU.
4. **Find plugins**.
5. Znajdź **Easy Looper** i oznacz go gwiazdką, żeby był na liście.

W FL na Macu możesz użyć **VST3 albo AU**. Zostaw oba; jeśli jeden się nie ładuje, użyj drugiego.

### 2. Audio: gitara → Scarlett → looper

1. **Options → Audio settings**
   - Device: Focusrite USB / Scarlett Solo
   - Sample rate: 44.1 kHz albo 48 kHz (plugin obsługuje oba)
2. Mixer: wybierz insert, na który idzie gitara.
3. Na tym tracku ustaw **input** na wejście instrumentu Scarlett (zwykle Input 2 / Inst).
4. W slotcie insertów tego tracka dodaj **Easy Looper**.
5. Track routuj na Master.
6. Włącz monitoring tracka (żeby słyszeć gitarę przez plugin).

Docelowy łańcuch:

`Gitara → Scarlett Solo → FL input → Easy Looper → Master`

Plugin zawsze przepuszcza wejście na wyjście. Loop dokłada się do tego sygnału w Playing / Overdubbing.

### 3. MIDI: M-VAVE Chocolate

Kontroler może wysyłać Note, CC albo PC — plugin **nie zakłada numerów**. Mapowanie robisz MIDI Learnem.

1. Podłącz Chocolate (USB albo Bluetooth MIDI).
2. **Options → MIDI settings**.
3. Włącz port **M-VAVE Chocolate** (Enable).
4. Controller type: **generic controller** / All.
5. Otwórz okno Easy Looper.
6. W wrapperze pluginu (zębatka FL) włącz odbieranie MIDI przez ten plugin:
   - **VST wrapper → Settings / MIDI**
   - Receive notes from: **all MIDI inputs** albo konkretnie Chocolate.

Jeśli pedały poruszają czymś w FL, a plugin nic nie pokazuje: MIDI idzie do FL, nie do insertu. Wtedy:

- w MIDI settings wyłącz „Send controller to focused plugin” jeśli przeszkadza, **albo**
- utwórz w Channel rack mały MIDI Out / użyj **typing to this plugin**, **albo**
- w mixerze: prawy klik na insertcie Easy Looper → powiąż MIDI input z tym pluginem.

Klucz: w GUI pluginu panel **LAST MIDI** musi się zmieniać, gdy naciskasz pedał. Dopóki Type/Channel/Number/Value nie skacze, FL nie dostarcza MIDI do pluginu.

### 4. MIDI Learn

1. Kliknij **Learn Record**.
2. Naciśnij pedał Record na Chocolate.
3. Status wraca do `MIDI LEARN: idle`, a LAST MIDI pokazuje dokładny komunikat.
4. Powtórz dla Play/Stop, Overdub, Undo, Clear — każdy pedał osobno.

Learn zapisuje type + channel + number. Komenda idzie tylko na **press**:

- Note On z velocity > 0
- CC z value > 0
- Program Change (jednorazowo)

Note Off / Note On velocity 0 / CC=0 **nie** odpalają komendy drugi raz.

Mapowanie żyje dopóki plugin jest załadowany. FL go nie zapisuje (presety są celowo poza MVP).

## Jak nagrywać loop

1. Status: `EMPTY`.
2. Pedał albo przycisk **RECORD** → `RECORDING` (graj).
3. Drugi raz **RECORD** → `PLAYING`, materiał zapętla się od razu.
4. **OVERDUB** → dokładasz warstwę; długość się nie zmienia. Drugi raz wraca do Playing.
5. **UNDO** → ostatni overdub znika.
6. **PLAY / STOP** → cisza loopa / z powrotem play (gitara nadal słychać).
7. **CLEAR** → zawsze `EMPTY`.

Nie ma syncu do BPM FL. Długość loopa = czas między dwoma Record.

## Debug

- Audio nie przechodzi: plugin nie jest na torze gitary albo input mixera jest zły.
- LAST MIDI martwe: routing MIDI, nie silnik loopera.
- Plugin niewidoczny: Manage plugins → Find plugins; sprawdź czy pliki są w `~/Library/Audio/Plug-Ins/`.
- macOS blokuje plugin: System Settings → Privacy & Security → Allow Anyway, potem reskan w FL.
