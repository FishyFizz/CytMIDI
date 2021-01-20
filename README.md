### 个人弄着玩的项目，有bug欢迎反馈
### Personally made this for fun. Bug reports and feedbacks are welcome.

# CytMIDI

A simple program used to convert MIDI file into Cytoid chart.

## Build Requirmets

Juce Framework

Visual Studio 2019

## Using CytMIDI:

### 1.Import your music track into music production software such as FL Studio / Cubase.

### 2.Create a new track and input MIDI notes. Rules are below:

####       a.A note that's longer than half of a beat AND is longer than 0.2s will be a HOLD note.
    
        a1.If the note holds till next time the scan line bounces back, the note will be a LONG HOLD (yelow one with a effect).
        
        a2.If the note ends before next time the scan line bounces back, the note will be a SHORT HOLD.
        
####      b.A note that's not on the default MIDI channel (= notes on channel 2~16) are CHAIN note.
    
        b1.Each channel of 2~16 represents a distinct chain.
        
        b2.When a first note on channel 2~16 is detected, the note is CHAIN HEAD.
        
        b3.A chain MUST HAVE a note in that specific channel with velocity 127(max) to indicate it's END.
        
        b4.If the note is not the first note detected, and it's not a CHAIN END, it's a CHAIN BODY note.
        
####      c.If a note doesn't fall in the categories above:
    
        c1.It's a TAP note if the velocity is below 120.
        
        c2.It's a FLICK note if the velocity is above or equal to 120.
        
####      d.The position of the note appearing in the chart is determined by the MIDI PITCH.
    
        d1.CytMIDI detects the LOWEST and HIGHEST pitch in the MIDI file.
        
        d2.The lowest and highest note will appear at the leftmost and rightmost position.
        
        d3.Notes between them will spread evenly according to their MIDI pitch.
        
### 3.Export MIDI file and drag-drop onto the program.

### 4.CytMIDI will ask for the beat number of a single page. A bigger number will resulting a slow scan line, and a crowdy chart. This is usually set to 1/2/4.

### 5.You should see CytMIDI showing the MIDI signal it read, and cytoid notes it translated.

### 6.Chart.json will be created in the working directory of the program.

### 7.import the chart into a editor and fine tune.


# CytMIDI

用于将MIDI文件直接转换为Cytoid谱面的程序

## 编译条件

Juce Framework

Visual Studio 2019

## CytMIDI使用说明：

### 1.把你的音乐导入FL Studio/Cubase等软件。

### 2.新建一个轨道，在轨道中输入MIDI音符。规则如下：

####       a.时值大于等于半拍【且】持续时间大于等于0.2秒的音符，将被识别为长按音符。
    
        a1.若这个音符持续到扫描线折返，那么这是黄色长按音符（有竖向贯穿屏幕特效）。
        
        a2.若这个音符没有持续到扫描线折返，那么这是普通长按音符（无特效）。
        
####      b.如果一个音符不在默认MIDI通道（即，若音符在2~16通道），那么这是锁链音符。
    
        b1.通道2~16各代表一个独立的锁链。
        
        b2.当首次检测到任意一个通道2~16的音符，这个音符是锁链起始。
        
        b3.一个锁链【必须以一个力度为127(最大)的音符来表示结束】。
        
        b4.如果不是首次检测到某个通道的音符，且该通道的锁链还未结束(参见b3)，那么这个音符是锁链中部。
        
####      c.如果一个音符不满足上面列出的条件，那么：
    
        c1.如果这个音符力度<120，这是一个点击音符。
        
        c2.如果这个音符力度>=120，这是一个滑动音符。
        
####      d.音符在谱面上的位置由音高决定。低音在左侧，高音在右侧。
    
        d1.CytMIDI会检测MIDI文件中的最高音和最低音。
        
        d2.最高音和最低音会分别被放在谱面的最左和最右侧。
        
        d3.在最高音和最低音之间的音符按照MIDI音高均匀分配在谱面上。
        
### 3.导出编辑好的MIDI文件，保存在纯英文路径下，并拖放到CytMIDI上。

### 4.CytMIDI将会问你每一页谱面包含多少拍。这个数越大扫描线越慢，音符的视觉效果越密集。常用的是1/2/4。

### 5.你应该可以看到CytMIDI显示读出的MIDI信号，并且显示识别出的音符。

### 6.CytMIDI会在程序所在的文件夹创建一个Chart.json，这就是谱面文件。

### 7.把谱面导入到编辑器并修改。
