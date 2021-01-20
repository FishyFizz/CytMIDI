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
        cout << R"(CytMIDIʹ��˵����
1.��������ֵ���FL Studio/Cubase�������
2.�½�һ��������ڹ��������MIDI�������������£�
    a.ʱֵ���ڵ��ڰ��ġ��ҡ�����ʱ����ڵ���0.2�������������ʶ��Ϊ����������
        a1.���������������ɨ�����۷�����ô���ǻ�ɫ����������������ᴩ��Ļ��Ч����
        a2.���������û�г�����ɨ�����۷�����ô������ͨ��������������Ч����
    b.���һ����������Ĭ��MIDIͨ����������������2~16ͨ��������ô��������������
        b1.ͨ��2~16������һ��������������
        b2.���״μ�⵽����һ��ͨ��2~16�����������������������ʼ��
        b3.һ��������������һ������Ϊ127(���)����������ʾ��������
        b4.��������״μ�⵽ĳ��ͨ�����������Ҹ�ͨ����������δ����(�μ�b3)����ô��������������в���
    c.���һ�����������������г�����������ô��
        c1.��������������<120������һ�����������
        c2.��������������>=120������һ������������
    d.�����������ϵ�λ�������߾�������������࣬�������Ҳࡣ
        d1.CytMIDI����MIDI�ļ��е���������������
        d2.��������������ֱ𱻷����������������Ҳࡣ
        d3.��������������֮�����������MIDI���߾��ȷ����������ϡ�
3.�����༭�õ�MIDI�ļ��������ڴ�Ӣ��·���£����Ϸŵ�CytMIDI�ϡ�
4.CytMIDI��������ÿһҳ������������ġ������Խ��ɨ����Խ�����������Ӿ�Ч��Խ�ܼ������õ���1/2/4��
5.��Ӧ�ÿ��Կ���CytMIDI��ʾ������MIDI�źţ�������ʾʶ�����������
6.CytMIDI���ڳ������ڵ��ļ��д���һ��Chart.json������������ļ���
7.�����浼�뵽�༭�����޸ġ�)" << endl;
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
