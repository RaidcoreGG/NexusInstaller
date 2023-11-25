using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NexusInstaller
{
	internal static class Program
	{
		/// <summary>
		///  The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
			Application.SetHighDpiMode(HighDpiMode.SystemAware);
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			string detectedPath = FindExecutablePath();
			
			Application.Run(new Form1(detectedPath));
		}

		private static string FindExecutablePath()
		{
			string executablePath = null;

			try
			{
				RegistryKey root64 = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
				RegistryKey root32 = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry32);

				RegistryKey registryKey = root64.OpenSubKey("SOFTWARE\\ArenaNet\\Guild Wars 2");
				if (registryKey == null)
				{
					registryKey = root32.OpenSubKey("SOFTWARE\\ArenaNet\\Guild Wars 2");
				}
				if (registryKey != null)
				{
					executablePath = registryKey.GetValue("Path").ToString();
					registryKey.Close();
				}

				root64.Close();
				root32.Close();

				if (string.IsNullOrWhiteSpace(executablePath) || !File.Exists(executablePath))
				{
					// Locate if necessary
					// Prompt installation if necessary
					DialogResult locate = MessageBox.Show("No valid installation detected.\nLocate game?", "NULL", MessageBoxButtons.YesNo);
					if (locate == DialogResult.Yes)
					{
						OpenFileDialog fd = new OpenFileDialog
						{
							CheckFileExists = true,
							Filter = "Executable files (*.exe)|*.exe"
						};

						if (fd.ShowDialog() == DialogResult.OK)
						{
							executablePath = fd.FileName;
						}
					}
					else
					{
						DialogResult install = MessageBox.Show("Install Guild Wars 2?", "NULL", MessageBoxButtons.YesNo);
						if (install == DialogResult.Yes)
						{
							string tempPath = "";
							using (WebClient client = new WebClient())
							{
								string os = "";
								if (Environment.Is64BitOperatingSystem) { os = "64"; }
								else { os = "32"; }
								tempPath = Path.Combine(Path.GetTempPath(), $"Gw2Setup-{os}.exe");
								client.DownloadFile($"https://account.arena.net/content/download/gw2/win/{os}", tempPath);
							}

							Process installer = new Process
							{
								StartInfo = new ProcessStartInfo(tempPath)
							};
							Process.Start(tempPath);
							installer.WaitForExit();
							return FindExecutablePath();
						}
					}
				}
			}
			catch (Exception exc)
			{
				Console.WriteLine(exc.Message);
			}

			return executablePath;
		}
	}
}
