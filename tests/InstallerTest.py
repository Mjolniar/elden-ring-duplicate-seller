"""Exercise installer in isolated fake launcher folders, never the live game."""
import ctypes, hashlib, json, os, pathlib, shutil, subprocess, sys, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
DLL=pathlib.Path(sys.argv.pop(1)).resolve()
BASE=ROOT/'dist'/'installer-tests'; BASE.mkdir(parents=True,exist_ok=True)
class InstallerTest(unittest.TestCase):
    def setUp(self):
        self.root=pathlib.Path(tempfile.mkdtemp(prefix='case-',dir=BASE))
        self.package=self.root/'package'; self.package.mkdir()
        for name in ('Installer.ps1','Configure.ps1','Install.cmd','Uninstall.cmd','Settings.cmd','sell_values.txt'):
            shutil.copyfile(ROOT/'release'/name,self.package/name)
        shutil.copyfile(DLL,self.package/'ERDuplicateSeller.dll')
        shutil.copyfile(ROOT/'config/provisional_2710.ini',self.package/'duplicate_seller.ini')
        self.manifest()
        self.target=self.root/"Mod Engine user's folder"; self.target.mkdir()
        (self.target/'modengine2_launcher.exe').touch()
        self.config=self.target/'config_eldenring.toml'
        self.original='''# my launcher\n[modengine]\ndebug = false\nexternal_dlls = [\n  "other\\\\mod.dll", # keep this comment\n  'another.dll',\n]\n[extension.mod_loader]\nenabled = true\nmods = [{ enabled = true, name = "custom", path = "randomizer" }]\n'''
        self.config.write_text(self.original,encoding='utf-8')
        self.driver=self.root/'driver.ps1'
        self.driver.write_text('''param($Installer,$Target,$Action,$Mode,$Game)\n$global:fakeGame=$Game\nfunction Get-Process { param($Name,$ErrorAction) if ($global:fakeGame -eq 'yes') { [pscustomobject]@{Id=123;ProcessName='eldenring'} } }\n& $Installer -TargetDirectory $Target -Action $Action -SaleMode $Mode -NonInteractive\n''',encoding='utf-8')
    def manifest(self):
        files=sorted(p for p in self.package.iterdir() if p.name!='SHA256SUMS.txt')
        (self.package/'SHA256SUMS.txt').write_text('\n'.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name for p in files)+'\n')
    def run_setup(self,action='Install',mode='Keep',game='no',ok=True):
        r=subprocess.run(['powershell.exe','-NoProfile','-File',str(self.driver),str(self.package/'Installer.ps1'),str(self.target),action,mode,game],capture_output=True,text=True)
        if ok: self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        else: self.assertNotEqual(r.returncode,0,r.stdout+r.stderr)
        return r
    def test_install_update_uninstall(self):
        self.run_setup(mode='Sell')
        text=self.config.read_text()
        self.assertEqual(text.count("'ERDuplicateSeller/ERDuplicateSeller.dll'"),1)
        self.assertIn('"other\\\\mod.dll"',text); self.assertIn('# keep this comment',text)
        self.assertEqual(text.split('[extension.mod_loader]')[1],self.original.split('[extension.mod_loader]')[1])
        folder=self.target/'ERDuplicateSeller'; ini=folder/'duplicate_seller.ini'
        self.assertIn('dry_run=false',ini.read_text()); self.assertIn('sell_value_path=sell_values.txt',ini.read_text())
        custom=ini.read_text().replace('keep_weapons=2','keep_weapons=5'); ini.write_text(custom)
        (folder/'sell_values.txt').write_text('# custom prices\nweapon 1000 500\n')
        self.run_setup()
        self.assertEqual(self.config.read_text(),text)
        self.assertEqual(ini.read_text(),custom)
        self.assertIn('# custom prices',(folder/'sell_values.txt').read_text())
        self.run_setup('Uninstall')
        self.assertNotIn('ERDuplicateSeller.dll',self.config.read_text())
        self.assertIn('another.dll',self.config.read_text())
        self.assertFalse(folder.exists())
        self.assertTrue(list((self.target/'.ERDuplicateSeller-backups').glob('*/ERDuplicateSeller/duplicate_seller.ini')))
    def test_missing_list(self):
        self.config.write_text('[modengine]\ndebug = false\n[extension.mod_loader]\nenabled = true\n')
        self.run_setup()
        self.assertIn('external_dlls',self.config.read_text())
    def test_duplicate_registration(self):
        self.config.write_text('[modengine]\nexternal_dlls=["D:\\\\old\\\\ERDuplicateSeller.dll", \'ERDuplicateSeller/ERDuplicateSeller.dll\', "keep.dll"]\n')
        self.run_setup()
        self.assertEqual(self.config.read_text().count('ERDuplicateSeller.dll'),1)
        self.assertIn('keep.dll',self.config.read_text())
    def test_reject_malformed_without_changes(self):
        for body in ('external_dlls=["a.dll" "b.dll"]','external_dlls=["a.dll"','external_dlls=[]\nexternal_dlls=[]','external_dlls=[,]'):
            text='[modengine]\n'+body+'\n'; self.config.write_text(text)
            self.run_setup(ok=False)
            self.assertEqual(self.config.read_text(),text)
            self.assertFalse((self.target/'ERDuplicateSeller').exists())
    def test_missing_section(self):
        self.config.write_text('[other]\nexternal_dlls=[]\n')
        self.run_setup(ok=False)
        self.assertFalse((self.target/'.ERDuplicateSeller-backups').exists())
    def test_corrupted_package(self):
        (self.package/'ERDuplicateSeller.dll').write_bytes(b'corrupt')
        self.run_setup(ok=False)
        self.assertEqual(self.config.read_text(),self.original)
        self.assertFalse((self.target/'ERDuplicateSeller').exists())
    def test_game_running(self):
        self.run_setup(game='yes',ok=False)
        self.assertEqual(self.config.read_text(),self.original)
        self.assertFalse((self.target/'.ERDuplicateSeller-backups').exists())
    def test_path_traversal_manifest(self):
        (self.package/'SHA256SUMS.txt').write_text('0'*64+'  ../escape.dll\n')
        self.run_setup(ok=False)
        self.assertFalse((self.target/'ERDuplicateSeller').exists())
    def test_rollback_on_locked_file(self):
        self.run_setup()
        folder=self.target/'ERDuplicateSeller'; dll=folder/'ERDuplicateSeller.dll'
        dll.write_bytes(b'previous version')
        original=self.config.read_bytes()
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        kernel.CreateFileW.restype=ctypes.c_void_p
        kernel.CloseHandle.argtypes=[ctypes.c_void_p]
        # Install has already replaced the DLL when it encounters this later file.
        locked=folder/'Install.cmd'
        handle=kernel.CreateFileW(str(locked),0x80000000,0,None,3,0,None)
        self.assertNotEqual(handle,ctypes.c_void_p(-1).value)
        try: self.run_setup(ok=False)
        finally: kernel.CloseHandle(handle)
        self.assertEqual(dll.read_bytes(),b'previous version')
        self.assertEqual(self.config.read_bytes(),original)
    def test_settings_without_ini_edit(self):
        self.run_setup()
        script=self.target/'ERDuplicateSeller/Configure.ps1'
        ini=self.target/'ERDuplicateSeller/duplicate_seller.ini'
        original=ini.read_text()
        for mode,value in (('Sell','false'),('Preview','true')):
            r=subprocess.run(['powershell.exe','-NoProfile','-File',str(script),'-Mode',mode],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stderr)
            self.assertEqual(ini.read_text(),original.replace('dry_run=true','dry_run='+value))
if __name__=='__main__': unittest.main()
