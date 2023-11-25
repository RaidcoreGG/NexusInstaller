namespace NexusInstaller
{
	partial class Form1
	{
		/// <summary>
		///  Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		///  Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		///  Required method for Designer support - do not modify
		///  the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			label1 = new System.Windows.Forms.Label();
			labelGameLocation = new System.Windows.Forms.Label();
			buttonLocateGame = new System.Windows.Forms.Button();
			label2 = new System.Windows.Forms.Label();
			labelDetectedMods = new System.Windows.Forms.Label();
			buttonInstall = new System.Windows.Forms.Button();
			buttonQuit = new System.Windows.Forms.Button();
			labelResult = new System.Windows.Forms.Label();
			SuspendLayout();
			// 
			// label1
			// 
			label1.AutoSize = true;
			label1.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point);
			label1.Location = new System.Drawing.Point(12, 9);
			label1.Name = "label1";
			label1.Size = new System.Drawing.Size(93, 15);
			label1.TabIndex = 0;
			label1.Text = "Game Location:";
			// 
			// labelGameLocation
			// 
			labelGameLocation.Location = new System.Drawing.Point(12, 24);
			labelGameLocation.Name = "labelGameLocation";
			labelGameLocation.Size = new System.Drawing.Size(440, 30);
			labelGameLocation.TabIndex = 1;
			labelGameLocation.Text = "-";
			// 
			// buttonLocateGame
			// 
			buttonLocateGame.Location = new System.Drawing.Point(12, 57);
			buttonLocateGame.Name = "buttonLocateGame";
			buttonLocateGame.Size = new System.Drawing.Size(440, 40);
			buttonLocateGame.TabIndex = 2;
			buttonLocateGame.Text = "Wrong path? Locate game.";
			buttonLocateGame.UseVisualStyleBackColor = true;
			buttonLocateGame.Click += buttonLocateGame_Click;
			// 
			// label2
			// 
			label2.AutoSize = true;
			label2.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point);
			label2.Location = new System.Drawing.Point(12, 112);
			label2.Name = "label2";
			label2.Size = new System.Drawing.Size(141, 15);
			label2.TabIndex = 3;
			label2.Text = "Detected Modifications:";
			// 
			// labelDetectedMods
			// 
			labelDetectedMods.Location = new System.Drawing.Point(12, 127);
			labelDetectedMods.Name = "labelDetectedMods";
			labelDetectedMods.Size = new System.Drawing.Size(440, 216);
			labelDetectedMods.TabIndex = 4;
			labelDetectedMods.Text = "-";
			// 
			// buttonInstall
			// 
			buttonInstall.Location = new System.Drawing.Point(12, 361);
			buttonInstall.Name = "buttonInstall";
			buttonInstall.Size = new System.Drawing.Size(215, 40);
			buttonInstall.TabIndex = 5;
			buttonInstall.Text = "Install";
			buttonInstall.UseVisualStyleBackColor = true;
			buttonInstall.Click += buttonInstall_Click;
			// 
			// buttonQuit
			// 
			buttonQuit.Location = new System.Drawing.Point(233, 361);
			buttonQuit.Name = "buttonQuit";
			buttonQuit.Size = new System.Drawing.Size(219, 40);
			buttonQuit.TabIndex = 6;
			buttonQuit.Text = "Quit";
			buttonQuit.UseVisualStyleBackColor = true;
			buttonQuit.Click += buttonQuit_Click;
			// 
			// labelResult
			// 
			labelResult.AutoSize = true;
			labelResult.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point);
			labelResult.ForeColor = System.Drawing.Color.Green;
			labelResult.Location = new System.Drawing.Point(12, 343);
			labelResult.Name = "labelResult";
			labelResult.Size = new System.Drawing.Size(217, 15);
			labelResult.TabIndex = 7;
			labelResult.Text = "Successfully installed Raidcore Nexus!";
			labelResult.Visible = false;
			// 
			// Form1
			// 
			AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			ClientSize = new System.Drawing.Size(464, 413);
			Controls.Add(labelResult);
			Controls.Add(buttonQuit);
			Controls.Add(buttonInstall);
			Controls.Add(labelDetectedMods);
			Controls.Add(label2);
			Controls.Add(buttonLocateGame);
			Controls.Add(labelGameLocation);
			Controls.Add(label1);
			FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedToolWindow;
			Name = "Form1";
			Text = "Raidcore Nexus Installer";
			ResumeLayout(false);
			PerformLayout();
		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label labelGameLocation;
		private System.Windows.Forms.Button buttonLocateGame;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label labelDetectedMods;
		private System.Windows.Forms.Button buttonInstall;
		private System.Windows.Forms.Button buttonQuit;
		private System.Windows.Forms.Label labelResult;
	}
}
