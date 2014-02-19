using System;
using System.Xml;
using System.Xml.XPath;
using System.IO;
using System.Collections.Generic;
using System.Text;

namespace CertCompressor
{
    class Program
    {
        /// <summary>
        /// Purpose: 
		/// - Read all PEM files and write them into one TXT file
		/// - Read mime types from XML file and convert to INI format
        /// </summary>
        static void Main(string[] args)
        {
            Console.SetBufferSize(100, 5000);

            StringBuilder s_Text = new StringBuilder(100000);

            String s_WorkDir = Directory.GetCurrentDirectory();
            int Pos = s_WorkDir.IndexOf("\\Converter\\");
            if (Pos < 0)
                throw new Exception("Invalid directory. This program must run in \"...\\Converter\\...\"");

            String s_RootDir = s_WorkDir.Substring(0, Pos);

            if (true) // Convert Root Certificates
            {
                String s_ScanDir = s_RootDir + "\\Converter\\RootCertificates";
                String s_TxtFile = s_RootDir + "\\Wrapper\\RootCA.txt";

                File.Delete(s_TxtFile);

                Console.WriteLine("**************************************");
                Console.WriteLine("Reading all *.pem files in:");
                Console.WriteLine(s_ScanDir);
                Console.WriteLine("**************************************");
                Console.WriteLine("");
                
                int Count = 0;
                int Err   = 0;
                foreach (String s_File in Directory.GetFiles(s_ScanDir, "*.pem"))
                {
                    Console.WriteLine("Reading " + Path.GetFileName(s_File));

                    s_Text.Append(Path.GetFileNameWithoutExtension(s_File));

                    bool b_HasBegin = false;
                    bool b_HasEnd   = false;

                    foreach (String s_Line in File.ReadAllLines(s_File))
                    {
                        if (s_Line.Contains("---BEGIN "))
                        {
                            b_HasBegin = true;
                            s_Text.Append("\n-----BEGIN CERTIFICATE-----\n");
                            continue;
                        }
                        if (s_Line.Contains("---END "))
                        {
                            b_HasEnd = true;
                            s_Text.Append("-----END CERTIFICATE-----\n");
                            break;
                        }

                        s_Text.Append(s_Line.Trim());
                        s_Text.Append('\n');
                    }

                    if (!b_HasBegin || !b_HasEnd)
                    {
                        Console.WriteLine("**** ERROR FILE INVALID ****");
                        Err ++;
                    }

                    Count ++;
                }

                Console.WriteLine("");
                Console.WriteLine("**************************************");
                if (Err > 0)
                {
                    Console.WriteLine(Err+ " files are invalid PEM files.");
                    Console.WriteLine("Operation aborted.");
                }
                else if (Count > 0)
                {
                    Console.WriteLine("Writing content of "+Count+" certificates to:");
                    Console.WriteLine(s_TxtFile);
                    File.WriteAllText(s_TxtFile, s_Text.ToString(), Encoding.UTF8);
                }
                Console.WriteLine("**************************************");
                Console.WriteLine("");
            }

            // #########################################################################

            if (true) // Convert Mime Types
            {
                s_Text.Length = 0;
                s_Text.Append('\n'); // Important: '\n' before first line!

                String s_XmlFile = s_RootDir + "\\Converter\\MimeTypes\\mime.xml";
                String s_IniFile = s_RootDir + "\\Wrapper\\MimeTypes.txt";

                File.Delete(s_IniFile);

                Console.WriteLine("");
                Console.WriteLine("**************************************");
                Console.WriteLine("Reading all mime types in:");
                Console.WriteLine(s_XmlFile);
                Console.WriteLine("**************************************");
                Console.WriteLine("");
                
                XmlDocument i_Doc = new XmlDocument();
                i_Doc.Load(s_XmlFile);

                int Count = 0;

                foreach (XmlNode i_Map in i_Doc.DocumentElement.ChildNodes)
                {
                    if (i_Map.Name != "mime-mapping")
                        continue;

                    String s_Ext  = null;
                    String s_Type = null;

                    foreach (XmlNode i_Subnode in i_Map.ChildNodes)
                    {
                        switch (i_Subnode.Name)
                        {
                            case "extension": s_Ext  = i_Subnode.InnerText; break;
                            case "mime-type": s_Type = i_Subnode.InnerText; break;
                        }
                    }

                    if (s_Ext == null || s_Type == null)
                        continue;

                    Console.WriteLine("Reading ." + s_Ext);
                    
                    s_Text.AppendFormat("{0}={1}\n", s_Ext, s_Type);
                    Count ++;
                }

                Console.WriteLine("");
                Console.WriteLine("**************************************");
                
                if (Count > 0)
                {
                    Console.WriteLine("Writing "+Count+" mime types to:");
                    Console.WriteLine(s_IniFile);
                    File.WriteAllText(s_IniFile, s_Text.ToString(), Encoding.ASCII);
                }
                Console.WriteLine("**************************************");
                Console.WriteLine("");
            }

            // #########################################################################

            if (true) // Get codepages supported by Windows
            {
                SortedList<String,int> i_Codepages = new SortedList<String,int>();

                foreach (EncodingInfo i_Info in Encoding.GetEncodings())
                {
                    Encoding i_Enc = i_Info.GetEncoding();

                    i_Codepages[i_Enc.BodyName.ToLower()] = i_Enc.CodePage;

                    if (i_Enc.BodyName == i_Enc.HeaderName)
                        continue;

                    i_Codepages[i_Enc.HeaderName.ToLower()] = i_Enc.CodePage;
                }

                s_Text.Length = 0;

                String s_OutFile = s_RootDir + "\\libvmime\\src\\vmime\\platforms\\windows\\windowsCodepages.hpp";

                File.Delete(s_OutFile);

                Console.WriteLine("");
                Console.WriteLine("**************************************");
                Console.WriteLine("Writing all Windows codepages to:");
                Console.WriteLine(s_OutFile);
                Console.WriteLine("**************************************");
                Console.WriteLine("");

                s_Text.AppendLine("// This file is auto-generated by Converter.exe");
                s_Text.AppendLine("");
                s_Text.AppendLine("#pragma once");
                s_Text.AppendLine("");
                s_Text.AppendLine("namespace vmime     {");
                s_Text.AppendLine("namespace platforms {");
                s_Text.AppendLine("namespace windows   {");
                s_Text.AppendLine("");
                s_Text.AppendLine("class windowsCodepages");
                s_Text.AppendLine("{");
                s_Text.AppendLine("public:");
                s_Text.AppendLine("    static int getByName(const char* s8_Name)");
                s_Text.AppendLine("    {");

                foreach (String s_Name in i_Codepages.Keys)
                {
                    AppendCodepage(s_Text, s_Name, i_Codepages[s_Name]);

                    if (s_Name.StartsWith("utf-") ||
                        s_Name.StartsWith("iso-"))
                        AppendCodepage(s_Text, s_Name.Remove(3,1), i_Codepages[s_Name]);

                    if (s_Name.StartsWith("windows-"))
                        AppendCodepage(s_Text, s_Name.Remove(7,1), i_Codepages[s_Name]);
                }

                s_Text.AppendLine("");
                s_Text.AppendLine("        throw exception(std::string(\"Unknown charset: \") + s8_Name);");
                s_Text.AppendLine("    }");
                s_Text.AppendLine("};");
                s_Text.AppendLine("");
                s_Text.AppendLine("} // windows");
                s_Text.AppendLine("} // platforms");
                s_Text.AppendLine("} // vmime");

               
                File.WriteAllText(s_OutFile, s_Text.ToString(), Encoding.ASCII);
            }

            // #########################################################################

            // This is required to assure that Visual Studio re-compiles the Resources, because
            // otherwise Visual Studio will not notice that MimeTypes.txt and RootCA.txt have been modified.
            File.SetLastWriteTime(s_RootDir + "\\DemoC++\\Resource.rc",   DateTime.Now);
            File.SetLastWriteTime(s_RootDir + "\\vmime.NET\\Resource.rc", DateTime.Now);

            // #########################################################################

            Console.WriteLine("Done...");
            Console.WriteLine("Press Enter!");
            Console.Read();
        }

        static void AppendCodepage(StringBuilder s_Text, String s_Name, int s32_Codepage)
        {
            String s_Space = new String(' ', Math.Max(0, 17 - s_Name.Length));

            s_Text.AppendFormat("        if (stricmp(s8_Name, \"{0}\"){1} == 0) return {2};\r\n", 
                                s_Name, s_Space, s32_Codepage);

            Console.WriteLine(s_Name);
        }
    }
}

