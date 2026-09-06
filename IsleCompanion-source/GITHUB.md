# Push this folder to GitHub

```bat
cd %USERPROFILE%\Desktop\IsleCompanion-source
git init
git add .
git commit --trailer "Co-authored-by: Cursor <cursoragent@cursor.com>" -m "Initial v2 source"
git branch -M main
git remote add origin https://github.com/YOUR_USER/YOUR_REPO.git
git push -u origin main
```

Attach the portable Windows zip to a GitHub Release (not to this source tree).
