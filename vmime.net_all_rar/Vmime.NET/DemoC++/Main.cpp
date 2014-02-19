
#include "stdafx.h"
#include "Resource.h"
#include "../Wrapper/cSmtp.hpp"
#include "../Wrapper/cPop3.hpp"
#include "../Wrapper/cImap.hpp"
#include <conio.h>
#include <iostream>
#include <windows.h>

// Console colors
#define WHITE   (FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define GREY    (FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CYAN    (FOREGROUND_GREEN | FOREGROUND_BLUE  | FOREGROUND_INTENSITY)
#define MAGENTA (FOREGROUND_RED   | FOREGROUND_BLUE  | FOREGROUND_INTENSITY)
#define YELLOW  (FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define GREEN   (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define RED     (FOREGROUND_RED   | FOREGROUND_INTENSITY)
#define BLUE    (FOREGROUND_BLUE  | FOREGROUND_INTENSITY)

using namespace vmime::wrapper;

// forward declarations
static void PrintMailToConsole(cEmailParser* pi_Email);
static void TraceCallback(const char* s8_Trace);
static void PrintW(WORD u16_Color, const WCHAR* u16_Format, ...);
static wstring ShortenString(wstring s_String, int s32_MaxLines);


enum eDemo
{
    Demo_SMTP,
    Demo_POP3,
    Demo_IMAP
};

int _tmain(int argc, _TCHAR* argv[])
{
    eDemo  e_Demo;
    WCHAR *u16_From, *u16_To, *u16_Server, *u16_User, *u16_Password;

    // 0 -> use default port
    vmime_uint16 u16_Port = 0;

    // See cCommon.hpp
    cCommon::eSecurity e_Security = cCommon::Secur_SSL;

    // If the server sends a certificate that is not signed with a root certificate:
    // true  -> only write an ERROR to the Trace output.
    // false -> throw an exception and do not send the email.
    bool b_AllowInvalidCertificate = true;

    const wchar_t* u16_Subject = L"vmime.NET Test Email";

    switch (1)
    {
    case 1: // requires SSL or TLS!
        e_Demo        = Demo_SMTP;
        u16_From      = L"John Miller <jmiller@gmail.com>";
        u16_To        = L"John Miller <jmiller@gmail.com>";
        u16_Server    = L"smtp.googlemail.com";      
        u16_User      = L"JoMiller";      
        u16_Password  = L"TopSecret";
        break;

    // IMPORTANT: 
    // Gmail has a very buggy POP3 behaviour: All emails can be downloaded only ONCE via POP3, 
    // no matter what setting you chose in your POP3/IMAP configuration! The emails are there in 
    // the web interface but pop.googlemail.com tells you that you have no emails in your Inbox. 
    // Or it may happen the opposite that POP3 shows you emails that have been deleted years ago (but only once)!
    case 2: // requires SSL!
        e_Demo        = Demo_POP3;
        u16_From      = NULL; // not used
        u16_To        = NULL;
        u16_Server    = L"pop.googlemail.com";
        u16_User      = L"JoMiller";      
        u16_Password  = L"TopSecret";
        break;

    case 3: // requires SSL!
        e_Demo        = Demo_IMAP;
        u16_From      = NULL; // not used
        u16_To        = NULL;
        u16_Server    = L"imap.googlemail.com";
        u16_User      = L"JoMiller";      
        u16_Password  = L"TopSecret";
        break;

    // ---------------------------------

    case 4: // requires SSL or TLS!
        e_Demo        = Demo_SMTP;
        u16_From      = L"John Miller <jmiller@yahoo.de>";
        u16_To        = L"John Miller <jmiller@yahoo.de>";
        u16_Server    = L"smtp.mail.yahoo.de";
        u16_User      = L"JoMiller";      
        u16_Password  = L"TopSecret";
        break;

    case 5: // requires SSL!
        e_Demo        = Demo_POP3;
        u16_From      = NULL; // not used
        u16_To        = NULL;
        u16_Server    = L"pop.mail.yahoo.de";
        u16_User      = L"JoMiller";      
        u16_Password  = L"TopSecret";
        break;

    // ---------------------------------

    case 6:
        e_Demo        = Demo_SMTP;
        u16_From      = L"John Miller <jmiller@gmx.de>";
        u16_To        = L"John Miller <jmiller@gmx.de>";
        u16_Server    = L"mail.gmx.de";
        u16_User      = L"jmiller@gmx.de";      
        u16_Password  = L"TopSecret";
        break;

    case 7:
        e_Demo        = Demo_POP3;
        u16_From      = NULL; // not used
        u16_To        = NULL;
        u16_Server    = L"pop.gmx.de";
        u16_User      = L"jmiller@gmx.de";      
        u16_Password  = L"TopSecret";
        break;

    default:
        PrintW(RED, L"Invalid switch() value in _tmain().");
        getch();
        return 0;
    }

    // --------------------------------------------------------------------------------------------

    vmime::SetTraceCallback(TraceCallback);

    WCHAR u16_Dir[1000];
    GetModuleFileNameW(NULL, u16_Dir, 1000); // Full path of DemoCpp.exe

    wstring s_ExePath = u16_Dir;
    int s32_Pos = s_ExePath.find(L"\\output");

    wstring s_RootDir = s_ExePath.substr(0, s32_Pos);

    // Increase console buffer for 3000 lines output with 200 chars per line
    COORD k_Size = {200, 3000}; 
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), k_Size);

    COORD k_Max = GetLargestConsoleWindowSize(GetStdHandle(STD_OUTPUT_HANDLE));

    SMALL_RECT k_Wnd = {0}; 
    k_Wnd.Right  = min(k_Max.X, 150) -4;
    k_Wnd.Bottom = min(k_Max.Y,  60) -4;
    SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &k_Wnd);

    // This is required for wprintf() to show umlauts correctly
    setlocale(LC_ALL, "English_United States.1252");

    // --------------------------------------------------------------------------------------------

    try
    {
        switch (e_Demo)
        {
            case Demo_SMTP:
            {
                PrintW(WHITE, L"*******************\n   C++ SMTP Demo\n*******************\n");
              
                // true  -> html + plain text message, 
                // false -> plain text only message
                bool b_UseHtml = true;   

                wstring s_Attach = s_RootDir + L"\\libvmime\\doc\\Readme.txt";
                wstring s_Embed  = s_RootDir + L"\\VmimeLogo.png";

                // ----- Composition ------

                cEmailBuilder i_Email(u16_From, u16_Subject, IDR_MIME_TYPES);

                i_Email.AddTo(u16_To);
                i_Email.SetPlainText(L"This email has been auto-generated by vmime.NET.\r\n"
                                     L"This is the plain message part.\r\n"
                                     L"This is Chinese: \x65B9\x8A00\x5730\x9EDE");
                                     
                i_Email.AddAttachment(s_Attach.c_str(), L"", L"");
                i_Email.SetHeaderField(cEmailBuilder::Head_Organization, L"ElmueSoft");

                if (b_UseHtml)
                {
                    i_Email.SetHtmlText(L"This email has been auto-generated by vmime.NET.<br>\r\n"
                                        L"This is the <b>HTML</b> message part.<br/><br/>\r\n"
                                        L"<img src=\"cid:VmimeLogo\"/><br/>\r\n"
                                        L"(This image is an embedded object)<br/><br/>\r\n"
                                        L"This is Chinese: <font color=blue>\x65B9\x8A00\x5730\x9EDE</font><br/><br/>");

                    i_Email.AddEmbeddedObject(s_Embed.c_str(), L"", L"VmimeLogo");
                }
                
                PrintW(YELLOW, L"%s\n", i_Email.Generate().c_str());

                // ----- Transport ------

                cSmtp i_Smtp(u16_Server, u16_Port, e_Security, b_AllowInvalidCertificate, IDR_ROOT_CA);
                i_Smtp.SetAuthData(u16_User, u16_Password);
                i_Smtp.Send(&i_Email);
                break;
            }
            case Demo_POP3:
            {
                PrintW(WHITE, L"*******************\n   C++ POP3 Demo\n*******************\n");

                // true -> delete all test emails from the server that were sent before by this program
                bool b_DeleteTestMails = false;

                cPop3 i_Pop3(u16_Server, u16_Port, e_Security, b_AllowInvalidCertificate, IDR_ROOT_CA);
                i_Pop3.SetAuthData(u16_User, u16_Password);

                int s32_EmailCount = i_Pop3.GetEmailCount();

                PrintW(YELLOW, L"Folder 'Inbox' has %d emails", s32_EmailCount);

                // Run the loop reverse to show first the newest emails.
                for (int M=s32_EmailCount-1; M>=0; M--)
                {
                    PrintW(WHITE, L"========================================================================");

                    GuardPtr<cEmailParser> i_Email = i_Pop3.FetchEmailAt(M); // Sends here the TOP command

                    PrintMailToConsole(i_Email.Ptr());

                    // Delete all Test emails on the server
                    if (b_DeleteTestMails && i_Email->GetSubject() == u16_Subject)
                    {
                        PrintW(MAGENTA, L"Deleting email on server.");
                        i_Email->Delete(); // Mark for deletion
                    }
                }

                i_Pop3.Close(); // Now expunge all messages that are marked for deletion
                break;
            }
            case Demo_IMAP:
            {
                PrintW(WHITE, L"*******************\n   C++ IMAP Demo\n*******************\n");

                // true -> delete all test emails from the server that were sent before by this program
                bool b_DeleteTestMails = false;

                cImap i_Imap(u16_Server, u16_Port, e_Security, b_AllowInvalidCertificate, IDR_ROOT_CA);
                i_Imap.SetAuthData(u16_User, u16_Password);

                // -------------

                vector<wstring> i_FolderList;
                i_Imap.EnumFolders(i_FolderList); // Sends the LIST command

                for (int i=0; i<(int)i_FolderList.size(); i++)
                {
                    PrintW(MAGENTA, L"IMAP Folder %2d: \"%s\"", i+1, i_FolderList.at(i).c_str());
                }

                i_Imap.SelectFolder(L"INBOX"); // Sends the SELECT command

                // -------------

                int s32_EmailCount = i_Imap.GetEmailCount();

                PrintW(YELLOW, L"Folder '%s' has %d emails", i_Imap.GetCurrentFolder().c_str(), s32_EmailCount);

                // Run the loop reverse to avoid M indexing an already deleted email
                // and to show first the newest emails.
                for (int M=s32_EmailCount-1; M>=0; M--)
                {
                    PrintW(WHITE, L"========================================================================");

                    GuardPtr<cEmailParser> i_Email = i_Imap.FetchEmailAt(M); // Sends here the FETCH HEADER command

                    PrintMailToConsole(i_Email.Ptr());

                    // Delete all Test emails on the server
                    if (b_DeleteTestMails && i_Email->GetSubject() == u16_Subject)
                    {
                        PrintW(MAGENTA, L"Deleting email on server.");
                        i_Email->Delete(); // Delete the email immediately
                    }
                }

                i_Imap.Close(); // Close the connection to the server
                break;
            }
        }
    }
    catch (std::exception& e)
    {
        PrintW(RED, L"%s", UNI(e.what()).c_str());
    }

    PrintW(GREEN, L"\nPress Enter!");
    getch();
}

void PrintMailToConsole(cEmailParser* pi_Email)
{
    vmime_uint32 u32_Email = pi_Email->GetIndex() +1;

    wstring s_Subject = pi_Email->GetSubject();
    PrintW(YELLOW, L"Email %u: Subject='%s'", u32_Email, s_Subject.c_str());

    wstring s_FromName; 
    wstring s_FromEmail = pi_Email->GetFrom(&s_FromName);
    PrintW(YELLOW, L"Email %u: From='%s', Name='%s'", u32_Email, s_FromEmail.c_str(), s_FromName.c_str());

    vmime::ref<const vmime::datetime> i_Date = pi_Email->GetDate();
    PrintW(YELLOW, L"Email %u: Date=%04d-%02d-%02d %02d:%02d (Zone %+d)", u32_Email, i_Date->getYear(), i_Date->getMonth(), i_Date->getDay(), i_Date->getHour(), i_Date->getMinute(), i_Date->getZone()/60);

    wstring s_Organization = pi_Email->GetOrganization();
    PrintW(YELLOW, L"Email %u: Organization='%s'", u32_Email, s_Organization.c_str());

    wstring s_UserAgent = pi_Email->GetUserAgent();
    PrintW(YELLOW, L"Email %u: UserAgent='%s'", u32_Email, s_UserAgent.c_str());

    vector<wstring> i_Emails;
    vector<wstring> i_Names;
    pi_Email->GetTo(&i_Emails, &i_Names);
    pi_Email->GetCc(&i_Emails, &i_Names);

    for (size_t i=0; i<i_Emails.size(); i++)
    {
        wstring s_ToEmail = i_Emails.at(i);
        wstring s_ToName  = i_Names .at(i);
        PrintW(YELLOW, L"Email %u: Recipient %u: To,Cc='%s', Name='%s'", u32_Email, i+1, s_ToEmail.c_str(), s_ToName.c_str());
    }

    vmime::net::message::Flags e_Flags = pi_Email->GetFlags();
    wstring s_Flags;
    if (e_Flags & vmime::net::message::FLAG_SEEN)    s_Flags += L"Seen ";
    if (e_Flags & vmime::net::message::FLAG_RECENT)  s_Flags += L"Recent ";
    if (e_Flags & vmime::net::message::FLAG_DELETED) s_Flags += L"Deleted ";
    if (e_Flags & vmime::net::message::FLAG_REPLIED) s_Flags += L"Replied ";
    if (e_Flags & vmime::net::message::FLAG_MARKED)  s_Flags += L"Marked ";
    if (e_Flags & vmime::net::message::FLAG_PASSED)  s_Flags += L"Passed ";
    if (e_Flags & vmime::net::message::FLAG_DRAFT)   s_Flags += L"Draft ";
    PrintW(YELLOW, L"Email %u: Flags=%s", u32_Email, s_Flags.c_str());

    // ------------------------------------------------------------------------------------------------

    vmime_uint32 u32_Size = pi_Email->GetSize();   // POP3: sends here the LIST command
    PrintW(YELLOW, L"Email %u: Size=%u'", u32_Email, u32_Size);

    // ------------------------------------------------------------------------------------------------

    wstring s_UID = pi_Email->GetUID();            // POP3: sends here the UIDL command
    PrintW(YELLOW, L"Email %u: UID='%s'", u32_Email, s_UID.c_str());

    // ------------------------------------------------------------------------------------------------

    if (u32_Size > 500000)
    {
        PrintW(MAGENTA, L"Body part skipped because email is bigger than 500 kByte.");
        return;
    }

    wstring s_Plain = pi_Email->GetPlainText();    // POP3: sends here the RETR command, IMAP: sends FETCH BODY command
    PrintW(YELLOW, L"Email %u: PlainText=", u32_Email);
    PrintW(CYAN,   L"%s", ShortenString(s_Plain, 20).c_str());

    wstring s_Html = pi_Email->GetHtmlText();
    PrintW(YELLOW, L"Email %u: HtmlText=", u32_Email);
    PrintW(GREEN,  L"%s", ShortenString(s_Html, 20).c_str());

    // --------

    vmime_uint32 u32_ObjCount = pi_Email->GetEmbeddedObjectCount();
    PrintW(YELLOW, L"Email %u has %u Embedded Objects", u32_Email, u32_ObjCount);

    wstring s_ObjId, s_ObjType;
    string  s_ObjData;
    vmime_uint32 u32_Object = 0;
    while (pi_Email->GetEmbeddedObjectAt(u32_Object++, s_ObjId, s_ObjType, s_ObjData))
    {
        PrintW(YELLOW, L"Email %u, Embedded Object %u: Id='%s', Type='%s', Size=%d bytes", u32_Email, u32_Object, s_ObjId.c_str(), s_ObjType.c_str(), (int)s_ObjData.size());
    }

    // --------

    vmime_uint32 u32_AttCount = pi_Email->GetAttachmentCount();
    PrintW(YELLOW, L"Email %u has %u Attachments", u32_Email, u32_AttCount);

    wstring s_AttName, s_AttType;
    string  s_AttData;
    vmime_uint32 u32_Attach = 0;
    while (pi_Email->GetAttachmentAt(u32_Attach++, s_AttName, s_AttType, s_AttData))
    {
        PrintW(YELLOW, L"Email %u, Attachment %u: Name='%s', Type='%s', Size=%d bytes", u32_Email, u32_Attach, s_AttName.c_str(), s_AttType.c_str(), (int)s_AttData.size());
    }
}


// Print Trace (Debug) output to the console and/or to DebugView
static void TraceCallback(const char* s8_Trace)
{
    if (true) // true == Console
    {
        // If a Trace output starts with "ERROR" -> print in red.
        // e.g. "ERROR: The server's certificate cannot be trusted...."
        WORD   u16_Color = (strncmp(s8_Trace, "ERROR", 5) == 0) ? RED : GREY; 
        PrintW(u16_Color, L"%s", UNI(s8_Trace).c_str());
    }
    else
    {
        // Use OutputDebugString in non-console applications.
        // Capture output with DebugView from www.sysinternals.com or in Visual Studio output pane.
        OutputDebugStringW(UNI(s8_Trace).c_str());
    }
}

// Print coloured console output
void PrintW(WORD u16_Color, const WCHAR* u16_Format, ...)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), u16_Color); 
    va_list  args;
    va_start(args,  u16_Format);
    vwprintf(u16_Format, args);
    wprintf(L"\n");
}


// Shorten a string to a maximum amount of lines
static wstring ShortenString(wstring s_String, int s32_MaxLines)
{
    int Pos = 0;
    for (int L=0; L<s32_MaxLines; L++)
    {
        Pos = s_String.find('\n', Pos);
        if (Pos < 0)
            return s_String;

        Pos += 1;
    }
    return s_String.substr(0, Pos) + L"\n....<CUT>";
}
