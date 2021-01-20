/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>

#include "CommonInclude.h"
#include "Util.h"
#include "Main.h"

using namespace juce;

int main(int argc, char* argv[])
{
    string MidiFileName;
    register bool DebugMode = true;
    cout << setprecision(2) << setiosflags(ios::fixed);
    //Didn't provide file.
    if (argc < 2)
    {
        cout << R"(CytMIDI使用说明：
1.把你的音乐导入FL Studio/Cubase等软件。
2.新建一个轨道，在轨道中输入MIDI音符。规则如下：
    a.时值大于等于半拍【且】持续时间大于等于0.2秒的音符，将被识别为长按音符。
        a1.若这个音符持续到扫描线折返，那么这是黄色长按音符（有竖向贯穿屏幕特效）。
        a2.若这个音符没有持续到扫描线折返，那么这是普通长按音符（无特效）。
    b.如果一个音符不在默认MIDI通道（即，若音符在2~16通道），那么这是锁链音符。
        b1.通道2~16各代表一个独立的锁链。
        b2.当首次检测到任意一个通道2~16的音符，这个音符是锁链起始。
        b3.一个锁链【必须以一个力度为127(最大)的音符来表示结束】。
        b4.如果不是首次检测到某个通道的音符，且该通道的锁链还未结束(参见b3)，那么这个音符是锁链中部。
    c.如果一个音符不满足上面列出的条件，那么：
        c1.如果这个音符力度<120，这是一个点击音符。
        c2.如果这个音符力度>=120，这是一个滑动音符。
    d.音符在谱面上的位置由音高决定。低音在左侧，高音在右侧。
        d1.CytMIDI会检测MIDI文件中的最高音和最低音。
        d2.最高音和最低音会分别被放在谱面的最左和最右侧。
        d3.在最高音和最低音之间的音符按照MIDI音高均匀分配在谱面上。
3.导出编辑好的MIDI文件，保存在纯英文路径下，并拖放到CytMIDI上。
4.CytMIDI将会问你每一页谱面包含多少拍。这个数越大扫描线越慢，音符的视觉效果越密集。常用的是1/2/4。
5.你应该可以看到CytMIDI显示读出的MIDI信号，并且显示识别出的音符。
6.CytMIDI会在程序所在的文件夹创建一个Chart.json，这就是谱面文件。
7.把谱面导入到编辑器并修改。)" << endl;
        system("pause");
        exit(0);
    }
    else
    {
        MidiFileName = argv[1];
    }
    

    //-----------PREPARE MIDI FILE INPUT------------------------------------------

    cout << "Opening " << MidiFileName << endl;

    //Initialize midi file object
    File FileObject(MidiFileName);
    FileInputStream midStream(FileObject);
    MidiFile MidiFileObject;
    MidiFileObject.readFrom(midStream, true);
    int TrackCount = MidiFileObject.getNumTracks();
    if (!TrackCount)
        ErrExit("File does not contain a MIDI track.");

    //Get midi time format
    short MidiTickPerBeat = MidiFileObject.getTimeFormat();
    if(MidiTickPerBeat <= 0)
        ErrExit("Unsupported MIDI time format.");

    //Basic setup for chart
    cout << "Please enter how many beats is in one page : ";
    int BeatsPerPage;
    cin >> BeatsPerPage;
    int CytoidTickPerBeat = DEFAULT_CYTOID_TICKPERBEAT;
    int CytoidTickPerPage = CytoidTickPerBeat * BeatsPerPage;


    //-----------PROCESSING MIDI EVENTS-------------------------------------------


    //The leftmost and rightmost notes are placed at Leftmost and Rightmost (variable defined in xPos calculate section)
    //Notes between them spread evenly across the x axis
    //These variables denote the corresponding midi notes
    int NoteMin = 1000, NoteMax = -1;

    //Use lambda to get rid of function arguments
    auto LocalEvtMidiTickToCytoid = [&](MidiMessageSequence::MidiEventHolder* pEvt)
    {
        return EvtMidiTickToCytoid(pEvt, MidiTickPerBeat, CytoidTickPerBeat);
    };
    auto LocalMidiTickToCytoid = [&](double MidiTick)
    {
        return MidiTickToCytoid(MidiTick, MidiTickPerBeat, CytoidTickPerBeat);
    };

    MidiMessageSequence MergedSeq;

    if (DebugMode)
        cout << "Start merging track events..." << endl;
    //Process every track and merge into single sequence
    for (int i = 0; i < TrackCount; i++)
    {
        const MidiMessageSequence* pMidiSeq;
        pMidiSeq = MidiFileObject.getTrack(i);
        int EventCount = pMidiSeq->getNumEvents();
        for (int j = 0; j < EventCount; j++)
        {
            auto pEvt = pMidiSeq->getEventPointer(j);
            if (pEvt->message.isTempoMetaEvent())
            {
                MergedSeq.addEvent(pEvt->message);
                if (DebugMode)
                    cout <<"Merge Events: "<< pEvt->message.getDescription() << endl;
            }
                
            else if (pEvt->message.isNoteOn())
            {
                MergedSeq.addEvent(pEvt->message);
                MergedSeq.addEvent(pEvt->noteOffObject->message);
                MergedSeq.updateMatchedPairs();
                if (DebugMode)
                {
                    cout << "Merge Events: " << pEvt->message.getDescription() << endl;
                    cout << "Merge Events: " << pEvt->noteOffObject->message.getDescription() << endl;
                }
            }
        }
    }
    
    if (DebugMode)
        cout << "Start generating chart data..." << endl;
    vector<TempoEvent> TempoEvents;
    vector<NoteEvent> NoteEvents;
    vector<int> NoteNumbers;
    double CurrentSecondsPerBeat;

    //ChainState indicates whether the channel is in chain state. Which starts with an arbitary note and ends with velocity 127.
    bool ChainState[17] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    //LastChainNote remembers the INDEX of last node of each chain.
    int LastChainNode[17]= { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    int EventCount = MergedSeq.getNumEvents();
    for (int i = 0; i < EventCount; i++)
    {
        auto pEvt = MergedSeq.getEventPointer(i);

        //Handle tempo event
        if (pEvt->message.isTempoMetaEvent())
        {
            CurrentSecondsPerBeat = pEvt->message.getTempoSecondsPerQuarterNote();
            TempoEvents.push_back({  LocalEvtMidiTickToCytoid(pEvt),
                                     int(60 / CurrentSecondsPerBeat)  });
            if (DebugMode)
                cout << "Tempo event:" << int(60 / CurrentSecondsPerBeat) << endl;
            continue;
        }
        
        //Ignore note off
        if (!pEvt->message.isNoteOn())
            continue;

        //Process note on event

        //Update boundaries
        NoteMin = min(NoteMin, pEvt->message.getNoteNumber());
        NoteMax = max(NoteMax, pEvt->message.getNoteNumber());
        NoteNumbers.push_back(pEvt->message.getNoteNumber());

        //Get note informations
        double NoteOnMidiTick = pEvt->message.getTimeStamp();
        double NoteOffMidiTick = pEvt->noteOffObject->message.getTimeStamp();
        double NoteLengthMidiTick = NoteOffMidiTick - NoteOnMidiTick;
        double NoteLengthBeat = NoteLengthMidiTick / MidiTickPerBeat;
        double NoteLengthSeconds = NoteLengthBeat * CurrentSecondsPerBeat;
        double NoteOffCytoidTick = LocalMidiTickToCytoid(NoteOffMidiTick);
        int NoteChannel = pEvt->message.getChannel();
        int NoteVelocity = pEvt->message.getVelocity();

        //Calculate which page the note is on
        double BeatPos = NoteOnMidiTick / MidiTickPerBeat;
        int PageIndex = -1;

        double tmp = BeatPos;
        while (tmp >= 0) { tmp -= BeatsPerPage; PageIndex++; }
        NoteEvent evt;

        //When note is not at channel 1, interpret as a chain.
        if (NoteChannel != 1)
        {
            //Chain head
            if (!ChainState[NoteChannel])
            {
                evt = { PageIndex,                                  //Page index
                        NOTETYPE_CHAIN_HEAD,                        //Note Type
                        int(NoteEvents.size() + 1),                 //Note ID
                        LocalEvtMidiTickToCytoid(pEvt),             //Tick
                        0,                                          //Note xPos, will be calculated later
                        0,                                          //Hold Tick
                        0                                           //Next chain note ID
                };
                NoteEvents.push_back(evt);

                ChainState[NoteChannel] = true;
                LastChainNode[NoteChannel] = NoteEvents.size() - 1;

                if (DebugMode)
                    cout << "Note at beat " << BeatPos << ",HEAD , channel " << NoteChannel << endl;
            }

            //Chain body or chain end.
            else if (ChainState[NoteChannel])
            {
                evt = { PageIndex,                                  //Page index
                        NOTETYPE_CHAIN_BODY,                        //Note Type
                        int(NoteEvents.size() + 1),                 //Note ID
                        LocalEvtMidiTickToCytoid(pEvt),             //Tick
                        0,                                          //Note xPos, will be calculated later
                        0,                                          //Hold Tick
                        0                                           //Next chain note ID, DETERMINED BY VELOCITY
                };

                bool ChainEnd = (NoteVelocity == 127);
                ChainState[NoteChannel] = !ChainEnd;

                //Next chain note ID is -1 if chain ends.
                evt.NextID = ChainEnd ? -1 : 0;
                NoteEvents.push_back(evt);

                //Link last node to this node.
                NoteEvents[LastChainNode[NoteChannel]].NextID = NoteEvents.size();
                LastChainNode[NoteChannel] = NoteEvents.size() - 1;
                if (DebugMode)
                    cout << "Note at beat " << BeatPos << ","<<(ChainEnd?"TAIL":"BODY")<<" , channel " << NoteChannel << endl;
            }
            continue;
        }

        //Determine whether to interpret as tap or hold note
        //Anything longer or equal to an 8th note AND the time is longer than 0.2s is regarded as a hold note
        bool isHold;
        //Floating point comparison, tolerance included
        isHold = NoteLengthBeat >= (0.499) && NoteLengthSeconds >= (0.199);

        //Generate information for hold note
        if (isHold)
        {
            //Distinguish long hold and short hold.
            //A long hold will at least hold on to next scan line bounce.

            double NextBounceCytoidTick = (PageIndex+1) * CytoidTickPerPage;
            bool isLong = NextBounceCytoidTick <= NoteOffCytoidTick;

            evt =   {   PageIndex,                                      //Page index
                        (isLong ? NOTETYPE_HOLD_LONG : NOTETYPE_HOLD),  //Note Type
                        int(NoteEvents.size() + 1),                     //Note ID
                        LocalEvtMidiTickToCytoid(pEvt),                 //Tick
                        0,                                              //Note xPos, will be calculated later
                        LocalMidiTickToCytoid(NoteLengthMidiTick),      //Hold Tick
                        0                                               //Next chain note ID
                    };
            if (DebugMode)
                cout << "Note at beat " << BeatPos << ",HOLD"<<(isLong?"L":"S")<<", for " << NoteLengthBeat << " beats." << endl;
        }
        //Generate information for short note
        else
        {
            //Interpret note with velocity >= 120 as flick note
            bool isFlick = pEvt->message.getVelocity() >= 120;
            evt =   {   PageIndex,                                  //Page index
                        (isFlick ? NOTETYPE_FLICK : NOTETYPE_TAP),  //Note Type
                        int(NoteEvents.size() + 1),                 //Note ID
                        LocalEvtMidiTickToCytoid(pEvt),             //Tick
                        0,                                          //Note xPos, will be calculated later
                        LocalMidiTickToCytoid(NoteLengthMidiTick),  //Hold Tick
                        0                                           //Next chain note ID
                    };
            if (DebugMode)
                cout << "Note at beat " << BeatPos << ","<<(isFlick?"FLICK":"TAP  ")<< endl;
        }

        //Add note data to list
        NoteEvents.push_back(evt);
    }

    //Calculate note xPos
    double Leftmost = 0.1;
    double Rightmost = 0.9;
    double UnitPos = (Rightmost - Leftmost) / (NoteMax - NoteMin);
    for (int i = 0; i < NoteEvents.size(); i++)
        NoteEvents[i].xPos = (NoteNumbers[i] - NoteMin) * UnitPos + Leftmost;

    //---------------------JSON FILE OUTPUT---------------------------------------
    ofstream Output("Chart.json", ios::out);

    //File header information
    Output << "{\n\t\"format_version\" : 0,\n\t\"time_base\" : " << CytoidTickPerBeat << ",\n\t\"start_offset_time\" : 0.0,\n\t\"page_list\" : [\n";
    
    //Chart pages information
    int LastPage = NoteEvents[NoteEvents.size() - 1].PageIndex;
    for (int i = 0; i < LastPage; i++)
        Output << GeneratePageData(CytoidTickPerPage * i, CytoidTickPerPage * (i + 1), i % 2 ? 1 : -1) << ",\n";
    Output << GeneratePageData(CytoidTickPerPage * LastPage, CytoidTickPerPage * (LastPage + 1), LastPage % 2 ? 1 : -1) << "\n\t],\n";

    //Chart tempo information
    Output << "\"tempo_list\" : [\n";
    for (int i = 0; i < TempoEvents.size() - 1; i++)
        Output << GenerateTempoData(TempoEvents[i]) << ",\n";
    Output << GenerateTempoData(TempoEvents[TempoEvents.size()-1]) << "\n\t],\n";

    //Chart notes information
    Output << "\"note_list\" : [\n";
    for (int i = 0; i < NoteEvents.size() - 1; i++)
        Output << GenerateNoteData(NoteEvents[i]) << ",\n";
    Output << GenerateNoteData(NoteEvents[NoteEvents.size()-1]) << "\n]\n}";

    Output.close();

    return 0;
}
