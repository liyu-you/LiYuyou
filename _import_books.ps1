$ErrorActionPreference = 'Stop'
$root = "$env:USERPROFILE\Documents\PureRefBooks"
$dl = "$root\_downloads"
$booksDir = "$root\books"
New-Item -ItemType Directory -Force -Path $booksDir | Out-Null

$colors = @(
  "#667eea,#764ba2", "#f093fb,#f5576c", "#4facfe,#00f2fe",
  "#43e97b,#38f9d7", "#fa709a,#fee140", "#30cfd0,#330867",
  "#a8edea,#fed6e3", "#ff9a9e,#fecfef", "#ffecd2,#fcb69f",
  "#84fab0,#8fd3f4", "#a1c4fd,#c2e9fb", "#d4fc79,#96e6a1"
)

# source file -> (title, author)
$books = @(
  @('红楼梦.txt','红楼梦','曹雪芹'),
  @('西游记.txt','西游记','吴承恩'),
  @('水浒传.txt','水浒传','施耐庵'),
  @('论语.txt','论语','孔子'),
  @('道德经.txt','道德经','老子'),
  @('孟子.txt','孟子','孟子')
)

# Read existing library.json
$libPath = "$root\library.json"
$lib = Get-Content $libPath -Raw -Encoding UTF8 | ConvertFrom-Json
# normalize books array to a list of PSObjects
$existing = @($lib.books)

$now = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss")
$rng = New-Object System.Random

$newBooks = @()
foreach ($b in $books) {
    $src = Join-Path $dl $b[0]
    $title = $b[1]
    $author = $b[2]
    $bytes = [System.IO.File]::ReadAllBytes($src)
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)

    # Strip Gutenberg header: keep from "*** START OF" onward
    $startIdx = $text.IndexOf("*** START OF")
    if ($startIdx -ge 0) {
        $nl = $text.IndexOf("`n", $startIdx)
        if ($nl -ge 0) { $text = $text.Substring($nl + 1) }
    }
    # Strip Gutenberg footer: cut at "*** END OF"
    $endIdx = $text.IndexOf("*** END OF")
    if ($endIdx -ge 0) {
        $text = $text.Substring(0, $endIdx)
    }
    $text = $text.Trim()

    # Convert to simplified: keep traditional as-is (program renders fine); no conversion needed.

    $destName = "$title`_$author.txt"
    $destPath = Join-Path $booksDir $destName
    [System.IO.File]::WriteAllText($destPath, $text, (New-Object System.Text.UTF8Encoding($false)))

    $chars = $text.Length
    $id = [guid]::NewGuid().ToString()
    $color = $colors[$rng.Next($colors.Length)]

    $obj = [PSCustomObject]@{
        addedAt  = $now
        author   = $author
        bookmarks = @()
        coverColor = $color
        filePath = "books/$destName"
        id       = $id
        lastRead = $now
        progress = 0
        title    = $title
        totalChars = $chars
    }
    $newBooks += $obj
    "imported $title  chars=$chars  -> $destName"
}

# Merge and write back
$all = @($existing) + $newBooks
$out = [PSCustomObject]@{ books = $all }
$json = $out | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($libPath, $json, (New-Object System.Text.UTF8Encoding($false)))
"library.json updated, total books = $($all.Count)"
