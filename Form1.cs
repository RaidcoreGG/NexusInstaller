using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace NexusInstaller
{
	public partial class Form1 : Form
	{
		private string _GameLocation;
		private string GameLocation
		{
			get { return _GameLocation; }
			set
			{
				_GameLocation = value;
				labelGameLocation.Text = value;
				DetectAddons();
			}
		}

		public Form1(string gameDirectory)
		{
			InitializeComponent();

			GameLocation = gameDirectory;
		}

		private void buttonLocateGame_Click(object sender, EventArgs e)
		{
			OpenFileDialog ofd = new OpenFileDialog();
			ofd.Filter = "Executables (*.exe)|*.exe";

			if (ofd.ShowDialog() == DialogResult.OK)
			{
				GameLocation = ofd.FileName;
			}
		}

		private void DetectAddons()
		{
			string gameDir = Path.GetDirectoryName(GameLocation);

			string d3d11 = Path.Combine(gameDir, "d3d11.dll");
			string d3d11_chainload = Path.Combine(gameDir, "d3d11_chainload.dll");
			string dxgi = Path.Combine(gameDir, "dxgi.dll");
			string bin64dxgi = Path.Combine(gameDir, "bin64/dxgi.dll");
			string bin64cefdxgi = Path.Combine(gameDir, "bin64/cef/dxgi.dll");
			string addonLoader = Path.Combine(gameDir, "addonLoader.dll");

			string addonInfo = "";

			if (File.Exists(d3d11)) { addonInfo += d3d11 + "\n"; }
			if (File.Exists(d3d11_chainload)) { addonInfo += d3d11_chainload + "\n"; }
			if (File.Exists(dxgi)) { addonInfo += dxgi + "\n"; }
			if (File.Exists(bin64dxgi)) { addonInfo += bin64dxgi + "\n"; }
			if (File.Exists(bin64cefdxgi)) { addonInfo += bin64cefdxgi + "\n"; }
			if (File.Exists(addonLoader)) { addonLoader += d3d11 + "\n"; }

			if (addonInfo == "")
			{
				addonInfo = "No modifications installed.";
			}

			labelDetectedMods.Text = addonInfo;
		}

		private void Install()
		{
			string gameDir = Path.GetDirectoryName(GameLocation);

			string d3d11 = Path.Combine(gameDir, "d3d11.dll");
			string d3d11_chainload = Path.Combine(gameDir, "d3d11_chainload.dll");
			string dxgi = Path.Combine(gameDir, "dxgi.dll");
			string bin64dxgi = Path.Combine(gameDir, "bin64/dxgi.dll");
			string bin64cefdxgi = Path.Combine(gameDir, "bin64/cef/dxgi.dll");
			string addonLoader = Path.Combine(gameDir, "addonLoader.dll");

			// move addonloader/arcdps/any other d3d11 wrapper
			if (File.Exists(addonLoader))
			{
				// FriendlyFire's addon loader does not chainload, so we just prompt to delete it
				if (File.Exists(d3d11_chainload))
				{
					DialogResult dr = MessageBox.Show("Unused d3d11_chainload found. Delete?", "Delete chainload?", MessageBoxButtons.YesNo);

					if (dr == DialogResult.Yes)
					{
						File.Delete(d3d11_chainload);
					}
					else
					{
						File.Move(d3d11_chainload, Path.Combine(d3d11_chainload, ".old"));
					}
				}

				// move FF's AL's d3d11 to chainload
				File.Move(d3d11, d3d11_chainload);
			}
			else if (File.Exists(d3d11))
			{
				FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(d3d11);
				if (fvi.ProductName == "ReShade")
				{
					File.Move(d3d11, dxgi);
				}
				else if (fvi.FileDescription == "arcdps")
				{
					// move arcdps to addons to be loaded by nexus
					File.Move(d3d11, Path.Combine(gameDir, "addons/arcdps.dll"));
				}
				else if (fvi.ProductName == "Nexus")
				{
					DialogResult dr = MessageBox.Show("Nexus is already installed!");
					if (dr == DialogResult.OK)
					{
						Environment.Exit(0);
					}
				}
				else
				{
					File.Move(d3d11, d3d11_chainload);
				}
			}

			// Install Nexus
			using (var client = new WebClient())
			{
				client.DownloadFile("https://api.raidcore.gg/d3d11.dll", d3d11);
			}

			FileUnblocker.Unblock(d3d11);

			// Move all detected Nexus addons to /addons
			List<string> dllFiles = Directory.GetFiles(gameDir, "*.dll").ToList();
			dllFiles.AddRange(Directory.GetFiles(Path.Combine(gameDir, "bin64"), "*.dll").ToList());

			foreach (string dll in dllFiles)
			{
				if (dll.Contains("CoherentUI64.dll")) { continue; }
				if (dll.Contains("d3dcompiler_43.dll")) { continue; }
				if (dll.Contains("ffmpegsumo.dll")) { continue; }
				if (dll.Contains("icudt.dll")) { continue; }
				if (dll.Contains("libEGL.dll")) { continue; }
				if (dll.Contains("libGLESv2.dll")) { continue; }

				IntPtr hMod = LoadLibrary(dll);

				if (hMod != IntPtr.Zero)
				{
					IntPtr getAddonDef = GetProcAddress(hMod, "GetAddonDef");

					if (getAddonDef != IntPtr.Zero)
					{
						FileInfo fi = new FileInfo(dll);

						string newLoc = Path.Combine(gameDir, "addons", fi.Name);

						File.Move(dll, newLoc);
					}

					Console.WriteLine("meme");
				}

				FreeLibrary(hMod);
			}

			labelResult.Visible = true;
		}

		[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		internal static extern IntPtr LoadLibrary(string lpFileName);

		[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		internal static extern bool FreeLibrary(IntPtr hLibModule);

		[DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
		internal static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

		// Credits to https://stackoverflow.com/a/6375373/17555290
		public class FileUnblocker
		{
			[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
			[return: MarshalAs(UnmanagedType.Bool)]
			private static extern bool DeleteFile(string name);

			public static bool Unblock(string fileName)
			{
				return DeleteFile(fileName + ":Zone.Identifier");
			}
		}

		private void buttonInstall_Click(object sender, EventArgs e)
		{
			Install();
		}

		private void buttonQuit_Click(object sender, EventArgs e)
		{
			Environment.Exit(0);
		}
	}
}
