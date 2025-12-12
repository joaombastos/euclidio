# OSC Mapping (atualizado)

**Todas as mensagens OSC devem ser enviadas para:**
- **IP**: `192.168.4.1` (IP do ESP32 no modo AP)
- **Porta**: `8000`
- Ou use mDNS: `Euclidio.local:8000`

## Paths OSC

### Sequenciador

#### Parâmetros Básicos
- `/sequencer/steps [value]` - Define número de steps (1-32)
- `/sequencer/hits [value]` - Define número de hits (1-32)
- `/sequencer/offset [value]` - Define offset (0-31)
- `/sequencer/note [value]` - Define nota MIDI (0-127)
- `/sequencer/velocity [value]` - Define velocity (0-127)
- `/sequencer/channel [value]` - Define canal MIDI (0-15)
- `/sequencer/resolution [value]` - Define resolução (1-4: 1/4, 1/8, 1/16, 1/32)
- `/sequencer/track [value]` - Seleciona track (0-7)
- `/sequencer/note_length [value]` - Define duração da nota em ms (50-700)

#### Controle de Playback
- `/sequencer/playstop [start] [stop]` - Mensagem única para controlar 
- `/sequencer/tempo [value]` - Define BPM (30-240). Valor negativo = modo SLAVE
- `/sequencer/dub` - (legacy) alterna enable/disable da track selecionada
- `/sequencer/dub/<n>` - Alterna enable/disable da track `n` (0..7) — novo formato para mappings por track

### Encoder
- `/encoder/double_click` - Simula duplo click do encoder
- `/encoder/long_press` - Simula long press do encoder

