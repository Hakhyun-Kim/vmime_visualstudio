
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using vmimeNET;

namespace DemoCSharp
{
    class Program
    {
        enum eDemo
        {
            SMTP,
            POP3,
            IMAP
        }

        static void Main(string[] args)
        {
            eDemo  e_Demo;
            String s_From, s_To, s_Server, s_User, s_Password;

            // 0 -> use default port
            UInt16 u16_Port  = 0;
            
            // This setting is used also for POP3 and IMAP, not only for SMTP !
            Smtp.eSecurity e_Security = Smtp.eSecurity.SSL;  

            // If the server sends a certificate that is not signed with a root certificate:
            // true  -> only write an ERROR to the Trace output.
            // false -> throw an exception and do not send the email.
            bool b_AllowInvalidCertificate = true;
            
            String s_Subject = "vmime.NET Test Email";          

            switch (1)
            {
            case 1: // requires SSL or TLS!
                e_Demo      = eDemo.SMTP;
                s_From      = "John Miller <jmiller@gmail.com>";
                s_To        = "John Miller <jmiller@gmail.com>";
                s_Server    = "smtp.googlemail.com";      
                s_User      = "JoMiller";      
                s_Password  = "TopSecret";
                break;

            // IMPORTANT: 
            // Gmail has a very buggy POP3 behaviour: All emails can be downloaded only ONCE via POP3, 
            // no matter what setting you chose in your POP3/IMAP configuration! The emails are there in 
            // the web interface but pop.googlemail.com tells you that you have no emails in your Inbox. 
            // Or it may happen the opposite that POP3 shows you emails that have been deleted years ago (but only once)!
            case 2: // requires SSL!
                e_Demo      = eDemo.POP3;
                s_From      = null; // not used
                s_To        = null;
                s_Server    = "pop.googlemail.com";
                s_User      = "JoMiller";      
                s_Password  = "TopSecret";
                break;

            case 3: // requires SSL!
                e_Demo      = eDemo.IMAP;
                s_From      = null; // not used
                s_To        = null;
                s_Server    = "imap.googlemail.com";
                s_User      = "JoMiller";      
                s_Password  = "TopSecret";
                break;

            // ---------------------------------

            case 4: // requires SSL or TLS!
                e_Demo      = eDemo.SMTP;
                s_From      = "John Miller <jmiller@yahoo.de>";
                s_To        = "John Miller <jmiller@yahoo.de>";
                s_Server    = "smtp.mail.yahoo.de";
                s_User      = "JoMiller";
                s_Password  = "TopSecret";
                break;

            case 5: // requires SSL!
                e_Demo      = eDemo.POP3;
                s_From      = null; // not used
                s_To        = null;
                s_Server    = "pop.mail.yahoo.de";
                s_User      = "JoMiller";
                s_Password  = "TopSecret";
                break;

            // ---------------------------------

            case 6:
                e_Demo      = eDemo.SMTP;
                s_From      = "John Miller <jmiller@gmx.de>";
                s_To        = "John Miller <jmiller@gmx.de>";
                s_Server    = "mail.gmx.de";
                s_User      = "jmiller@gmx.de";
                s_Password  = "TopSecret";
                break;

            case 7:
                e_Demo      = eDemo.POP3;
                s_From      = null; // not used
                s_To        = null;
                s_Server    = "pop.gmx.de";
                s_User      = "jmiller@gmx.de";
                s_Password  = "TopSecret";
                break;

            default:
                Print(RED, "Invalid switch() value in Main().");
                Console.Read();
                return;
            }

            // --------------------------------------------------------------------------------------------

            vmimeNET.Common.SetTraceCallback(TraceCallback);

            // Increase console buffer for 3000 lines output with 200 chars per line
            Console.SetBufferSize(200, 3000);
            
            Console.WindowWidth  = Math.Min(Console.LargestWindowWidth, 150) -4;
            Console.WindowHeight = Math.Min(Console.LargestWindowHeight, 60) -4;

            String s_CurDir  = System.Environment.CurrentDirectory;
            int    s32_Pos   = s_CurDir.IndexOf("\\output");
            String s_RootDir = s_CurDir.Substring(0, s32_Pos);

            // --------------------------------------------------------------------------------------------

            try
            {
                switch (e_Demo)
                {
                    case eDemo.SMTP:
                    {
                        Print(WHITE, "*******************\n    C# SMTP Demo\n*******************\n\n");

                        // true  -> html + plain text message, 
                        // false -> plain text only message
                        bool b_UseHtml = true;   

                        String s_Attach = s_RootDir + "\\libvmime\\doc\\Readme.txt";
                        String s_Embed  = s_RootDir + "\\VmimeLogo.png";

                        // ----- Composition ------

                        using (EmailBuilder i_Email = new EmailBuilder(s_From, s_Subject))
                        {
                            i_Email.AddTo(s_To);
                            i_Email.SetPlainText("This email has been auto-generated by vmime.NET.\r\n" +
                                                 "This is the plain message part.\r\n" +
                                                 "This is Chinese: \x65B9\x8A00\x5730\x9EDE");
                                                 
                            i_Email.AddAttachment(s_Attach, "", "");
                            i_Email.SetHeaderField(EmailBuilder.eHeaderField.Organization, "ElmueSoft");

                            if (b_UseHtml)
                            {
                                i_Email.SetHtmlText ("This email has been auto-generated by vmime.NET.<br/>\r\n" +
                                                     "This is the <b>HTML</b> message part.<br/><br/>\r\n" +
                                                     "<img src=\"cid:VmimeLogo\"/><br/>\r\n" +
                                                     "(This image is an embedded object)<br/><br/>\r\n" +
                                                     "This is Chinese: <font color=blue>\x65B9\x8A00\x5730\x9EDE</font><br/><br/>");

                                i_Email.AddEmbeddedObject(s_Embed, "", "VmimeLogo");
                            }
                            
                            Print(YELLOW, "{0}\n\n", i_Email.Generate());

                            // ----- Transport ------

                            using (Smtp i_Smtp = new Smtp(s_Server, u16_Port, e_Security, b_AllowInvalidCertificate))
                            {
                                i_Smtp.SetAuthData(s_User, s_Password);
                                i_Smtp.Send(i_Email);
                            } // i_Smtp.Dispose()

                        } // i_Email.Dispose()                 
                        break;
                    }
                    case eDemo.POP3:
                    {
                        Print(WHITE, "*******************\n   C# POP3 Demo\n*******************\n\n");

                        // true -> delete all test emails from the server that were sent before by this program
                        bool b_DeleteTestMails = false;

                        using(Pop3 i_Pop3 = new Pop3(s_Server, u16_Port, (Pop3.eSecurity)e_Security, b_AllowInvalidCertificate))
                        {
                            i_Pop3.SetAuthData(s_User, s_Password);

                            int s32_EmailCount = i_Pop3.GetEmailCount();

                            Print(YELLOW, "Folder 'Inbox' has {0} emails", s32_EmailCount);

                            // Run the loop reverse to show first the newest emails.
                            for (int M=s32_EmailCount-1; M>=0; M--)
                            {
                                Print(WHITE, "========================================================================");

                                using (EmailParser i_Email = i_Pop3.FetchEmailAt(M)) // Sends here the TOP command
                                {
                                    PrintMailToConsole(i_Email);

                                    // Delete all Test emails on the server
                                    if (b_DeleteTestMails && i_Email.GetSubject() == s_Subject)
                                    {
                                        Print(MAGENTA, "Deleting email on server.\n");
                                        i_Email.Delete(); // Mark for deletion
                                    }
                                } // i_Email.Dispose()
                            }
                            i_Pop3.Close(); // Now expunge all messages that are marked for deletion

                        } // i_Pop3.Dispose()
                        break;
                    }
                    case eDemo.IMAP:
                    {
                        Print(WHITE, "*******************\n   C# IMAP Demo\n*******************\n");

                        // true -> delete all test emails from the server that were sent before by this program
                        bool b_DeleteTestMails = false;

                        using (Imap i_Imap = new Imap(s_Server, u16_Port, (Imap.eSecurity)e_Security, b_AllowInvalidCertificate))
                        {
                            i_Imap.SetAuthData(s_User, s_Password);

                            // -------------

                            int F=0;
                            foreach (String s_Folder in i_Imap.EnumFolders()) // Sends the LIST command
                            {
                                Print(MAGENTA, "IMAP Folder {0:D2}: \"{1}\"", ++F, s_Folder);
                            }

                            i_Imap.SelectFolder("INBOX"); // Sends the SELECT command

                            // -------------

                            int s32_EmailCount = i_Imap.GetEmailCount();

                            Print(YELLOW, "Folder '{0}' has {1} emails", i_Imap.GetCurrentFolder(), s32_EmailCount);

                            // Run the loop reverse to avoid M indexing an already deleted email
                            // and to show first the newest emails.
                            for (int M=s32_EmailCount-1; M>=0; M--)
                            {
                                Print(WHITE, "========================================================================");

                                using (EmailParser i_Email = i_Imap.FetchEmailAt(M)) // Sends here the FETCH HEADER command
                                {
                                    PrintMailToConsole(i_Email);

                                    // Delete all Test emails on the server
                                    if (b_DeleteTestMails && i_Email.GetSubject() == s_Subject)
                                    {
                                        Print(MAGENTA, "Deleting email on server.");
                                        i_Email.Delete(); // Delete the email immediately
                                    }
                                } // i_Email.Dispose()
                            }
                            i_Imap.Close(); // Close the connection to the server

                        } // i_Imap.Dispose()
                        break;
                    }
                }
            }
            catch (Exception Ex)
            {
                Print(RED, Ex.Message);
            }

            Print(GREEN, "\nPress Enter!\n");
            Console.Read();
        }

        static void PrintMailToConsole(EmailParser i_Email)
        {
            UInt32 u32_Email = i_Email.GetIndex() +1;

            String s_Subject = i_Email.GetSubject();
            Print(YELLOW, "Email {0}: Subject='{1}'", u32_Email, s_Subject);

            String[] s_From = i_Email.GetFrom();
            Print(YELLOW, "Email {0}: From='{1}', Name='{2}'", u32_Email, s_From[0], s_From[1]);

            int s32_Timezone;
            DateTime i_Date = i_Email.GetDate(out s32_Timezone);
            Print(YELLOW, "Email {0}: Date={1} (Zone {2})", u32_Email, i_Date.ToString("yyyy-MM-dd HH:mm"), s32_Timezone/60);

            String s_Organization = i_Email.GetOrganization();
            Print(YELLOW, "Email {0}: Organization='{1}'", u32_Email, s_Organization);

            String s_UserAgent = i_Email.GetUserAgent();
            Print(YELLOW, "Email {0}: UserAgent='{1}'", u32_Email, s_UserAgent);

            List<String> i_Emails = new List<String>();
            List<String> i_Names  = new List<String>();
            i_Email.GetTo(i_Emails, i_Names);
            i_Email.GetCc(i_Emails, i_Names);

            for (int i=0; i<i_Emails.Count; i++)
            {
                String s_ToEmail = i_Emails[i];
                String s_ToName  = i_Names [i];

                Print(YELLOW, "Email {0}: Recipient {1}: To,Cc='{2}', Name='{3}'", u32_Email, i+1, s_ToEmail, s_ToName);
            }

            EmailParser.eFlags e_Flags = i_Email.GetFlags();
            Print(YELLOW, "Email {0}: Flags={1}", u32_Email, e_Flags);

            // ------------------------------------------------------------------------------------------------

            UInt32 u32_Size = i_Email.GetSize();        // POP3: sends here the LIST command
            Print(YELLOW, "Email {0}: Size={1}'", u32_Email, u32_Size);

            // ------------------------------------------------------------------------------------------------

            String s_UID = i_Email.GetUID();            // POP3: sends here the UIDL command
            Print(YELLOW, "Email {0}: UID='{1}'", u32_Email, s_UID);

            // ------------------------------------------------------------------------------------------------

            if (u32_Size > 500000)
            {
                Print(MAGENTA, "Body part skipped because email is bigger than 500 kByte.");
                return;
            }

            String s_Plain = i_Email.GetPlainText();    // POP3: sends here the RETR command, IMAP sends FETCH BODY command
            Print(YELLOW, "Email {0}: PlainText=", u32_Email);
            Print(CYAN,   "{0}", ShortenString(s_Plain, 20));

            String s_Html = i_Email.GetHtmlText();
            Print(YELLOW, "Email {0}: HtmlText=", u32_Email);
            Print(GREEN,  "{0}", ShortenString(s_Html, 20));

            // --------

            UInt32 u32_ObjCount = i_Email.GetEmbeddedObjectCount();
            Print(YELLOW, "Email {0} has {1} Embedded Objects", u32_Email, u32_ObjCount);

            String  s_ObjId, s_ObjType;
            Byte[] u8_ObjData;
            UInt32 u32_Object = 0;
            while (i_Email.GetEmbeddedObjectAt(u32_Object++, out s_ObjId, out s_ObjType, out u8_ObjData))
            {
                Print(YELLOW, "Email {0}, Embedded Object {1}: Id='{2}', Type='{3}', Size={4} bytes", u32_Email, u32_Object, s_ObjId, s_ObjType, u8_ObjData.Length);
            }

            // --------

            UInt32 u32_AttCount = i_Email.GetAttachmentCount();
            Print(YELLOW, "Email {0} has {1} Attachments", u32_Email, u32_AttCount);

            String  s_AttName, s_AttType;
            Byte[] u8_AttData;
            UInt32 u32_Attach = 0;
            while (i_Email.GetAttachmentAt(u32_Attach++, out s_AttName, out s_AttType, out u8_AttData))
            {
                Print(YELLOW, "Email {0}, Attachment {1}: Name='{2}', Type='{3}', Size={4} bytes", u32_Email, u32_Attach, s_AttName, s_AttType, u8_AttData.Length);
            }
        }

        #region Console Extension

        [DllImport("kernel32.dll", EntryPoint="SetConsoleTextAttribute")]
        private static extern bool SetConsoleTextAttribute(IntPtr h_ConsoleOutput, Int16 u16_Attributes);

        [DllImport("kernel32.dll", EntryPoint="GetStdHandle")]
        private static extern IntPtr GetStdHandle(int u32_Device);

        const Int16 GREY    = 0x7;
        const Int16 BLUE    = 0x9;
        const Int16 GREEN   = 0xA;
        const Int16 CYAN    = 0xB;
        const Int16 RED     = 0xC;
        const Int16 MAGENTA = 0xD;
        const Int16 YELLOW  = 0xE;
        const Int16 WHITE   = 0xF;

        /// <summary>
        /// The guys at Microsoft forgot to implement coloured console output into .NET framework
        /// </summary>
        static private void Print(Int16 s16_Color, String s_Format, params Object[] o_Param)
        {
            Print(s16_Color, String.Format(s_Format, o_Param));
        }
        static private void Print(Int16 s16_Color, String s_Text)
        {
            const int STD_OUTPUT_HANDLE = -11;

            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), s16_Color); 
            Console.WriteLine(s_Text);
        }

        #endregion

        #region Trace Callback

        static void TraceCallback(String s_Trace)
        {
            Int16 s16_Color = s_Trace.StartsWith("ERROR") ? RED : GREY;
            Print(s16_Color, s_Trace);
        }

        #endregion

        #region Utilities

        // Shorten a string to a maximum amount of lines
        static String ShortenString(String s_String, int s32_MaxLines)
        {
            int Pos = 0;
            for (int L=0; L<s32_MaxLines; L++)
            {
                Pos = s_String.IndexOf('\n', Pos);
                if (Pos < 0)
                    return s_String;

                Pos += 1;
            }
            return s_String.Substring(0, Pos) + "\n....<CUT>";
        }

        #endregion
    }
}
