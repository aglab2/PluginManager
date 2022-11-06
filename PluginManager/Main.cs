using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PluginManager
{
    public partial class Main : Form
    {
        public Main()
        {
            InitializeComponent();
        }
        private bool HasAccessTo(string dir, FileSystemRights right)
        {
            try
            {
                var rules = Directory.GetAccessControl(dir).GetAccessRules(true, true, typeof(System.Security.Principal.SecurityIdentifier));
                WindowsIdentity identity = WindowsIdentity.GetCurrent();

                foreach (FileSystemAccessRule rule in rules)
                {
                    if (identity.Groups.Contains(rule.IdentityReference))
                    {
                        if ((right & rule.FileSystemRights) == right)
                        {
                            if (rule.AccessControlType == AccessControlType.Allow)
                                return true;
                        }
                    }
                }
            }
            catch { }
            return false;
        }

        private void button1_Click(object sender, EventArgs e)
        {
            var procs = Process.GetProcessesByName("project64");
            if (procs.Count() == 0)
            {
                MessageBox.Show("Cannot find Project64. Make sure it is launched", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (procs.Count() > 1)
            {
                MessageBox.Show("Too many Project64s is launched. Picking first one.", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }

            var proc = procs.First();
            var pj64Dir = Directory.GetParent(proc.MainModule.FileName).FullName;

            bool canWrite = HasAccessTo(pj64Dir, FileSystemRights.Write);
            if (!canWrite)
            {
                MessageBox.Show("No permissions to write to Project64 folder. Launching with admin rights", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Error);
                var exeName = System.Diagnostics.Process.GetCurrentProcess().MainModule.FileName;
                ProcessStartInfo startInfo = new ProcessStartInfo(exeName);
                startInfo.Verb = "runas";
                try
                {
                    Process.Start(startInfo);
                    Close();
                    return;
                }
                catch
                { }

                MessageBox.Show("Failed to launch with admin rights.", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            OpenFileDialog openFileDialog = new OpenFileDialog
            {
                Filter = "ZIP (*.zip;*.dll)|*.zip;*.dll|All files (*.*)|*.*",
                FilterIndex = 1,
            };

            if (openFileDialog.ShowDialog() != DialogResult.OK)
                return;

            proc.Kill();
            proc.WaitForExit();

            try
            {
                var pluginPath = openFileDialog.FileName;
                var pluginExt = Path.GetExtension(pluginPath);
                if (pluginExt == ".dll")
                {
                    var pluginName = Path.GetFileName(pluginPath);
                    var pj64PluginsDir = Path.Combine(pj64Dir, "Plugin");
                    var pj64PluginNewPath = Path.Combine(pj64PluginsDir, pluginName);
                    try
                    {
                        File.Delete(pj64PluginNewPath);
                    }
                    catch { }

                    File.Copy(pluginPath, pj64PluginNewPath);
                }
                else if (pluginExt == ".zip")
                {
                    // Do heuristics to figure out how plugin needs to be installed
                    var pluginZip = ZipFile.Open(pluginPath, ZipArchiveMode.Read);

                    bool hasPluginEntry = false;
                    foreach (var entry in pluginZip.Entries)
                    {
                        if (entry.FullName.StartsWith("Plugin"))
                        {
                            hasPluginEntry = true;
                        }
                    }

                    var installDir = hasPluginEntry ? pj64Dir : Path.Combine(pj64Dir, "Plugin");

                    // Cleanse all the old files
                    foreach (var entry in pluginZip.Entries)
                    {
                        if (0 == entry.Name.Count())
                            continue;

                        try
                        {
                            var oldEntryPath = Path.Combine(installDir, entry.FullName);
                            File.Delete(oldEntryPath);
                        }
                        catch { }
                    }

                    // Do it
                    pluginZip.ExtractToDirectory(installDir);
                }
                else
                {
                    throw new ArgumentException("Unknown extension");
                }

                MessageBox.Show("Plugin installed successfully!", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to install plugin!: {ex}", "Plugin Manager", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
        }
    }
}
