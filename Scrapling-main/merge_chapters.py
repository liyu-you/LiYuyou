# -*- coding: utf-8 -*-
"""将 我不是戏神_免费章节 目录下的 chapter_01.txt~chapter_80.txt 合并为 book_free_chapters.md
校验：每章文件存在、非空、首行标题与预期一致。生成带书名/作者/来源/说明/目录的 Markdown 文档。"""
import os
import sys

BASE = r"C:\Users\LiyuYou\Desktop\Q\Scrapling-main\我不是戏神_免费章节"
MD_PATH = os.path.join(BASE, "book_free_chapters.md")

TITLES = [
    "穿越的小小后遗症，横漂的脆皮青年",
    "原来我不是神经病，我是真的有系统！",
    "总有人倒在追梦的路上",
    "高导的介绍信，《射雕英雄传》？",
    "高导的人情，准备考个证",
    "他，要侵犯我！",
    "特约演员证，收获定向属性点",
    "张记中：偷偷的看你试镜",
    "一个有意思的野路子！",
    "山顶上，是怎么样的？",
    "亡国奴",
    "小人物",
    "原来，是他啊",
    "一遍过，好镜头！",
    "什么病都不如穷病厉害",
    "你来当郭靖！（武打戏限定）",
    "他的背影，恍然的错觉，就真的像郭靖",
    "入戏小诀窍，看着你，但想着他",
    "落魄与希望",
    "我们，不会废一辈子的！",
    "如此充实的规划",
    "未来，我也是大明星！",
    "那个群演，你过来一下",
    "他不是纸老虎啊！",
    "特供版本的特供待遇",
    "就算是花瓶，也要是最优秀的花瓶",
    "是杀，是仇",
    "他演的，更像活生生的人",
    "张卫建：能不能打我一拳",
    "一墙之隔的差别",
    "刘亦妃",
    "一名合格的群演？",
    "接戏的秘密武器",
    "恶因，恶果！",
    "杀人犯，孙果",
    "他是李轩，他是李轩！！",
    "一个值得尊重的群演",
    "写剧本的，确实没我这个穿剧本的懂",
    "他，武打很专业的",
    "演员，群演",
    "萍水相逢",
    "演艺圈的剑宗和气宗",
    "给年轻人一个机会",
    "《寻枪》",
    "地道的明星梦",
    "人物小记（求追读！！！很重要！！）",
    "对差生的优越感",
    "他似乎，就不用你教",
    "同框不落下风？群演？真的假的？",
    "江文的沉迷和上瘾",
    "我很中意你啊",
    "我，拒绝",
    "跨入新的一年，准备跨入新的台阶！",
    "艺考报名",
    "进步空间，发声技巧，周讯的指导",
    "姐宠了",
    "考证，属性来来来",
    "属性三选一",
    "艺考前夕",
    "所谓明星学生",
    "艺考第一轮",
    "王劲崧：太骇人了",
    "还有一个有意思的考生",
    "这你也会？",
    "集体小品《工地》",
    "考试结束，静待成绩，试镜《无间道》",
    "大陆版的张翠山，不像演的，像穿越来的",
    "角色理解",
    "这一届北电艺考有个三轮满分的狠人",
    "后悔了！！",
    "这个人是谁？？？",
    "物美价廉的宋青书",
    "《寻枪》的首映",
    "你管这叫横店圈的群演？",
    "你以后，大概会成为超厉害的演员",
    "吴晶：听君一席话，让我茅塞顿开",
    "《金粉世家》的刘亦妃",
    "朴木：我歌歌呢！",
    "状元郎",
    "北电状元，奖励属性，健康+剧本",
]

assert len(TITLES) == 80, "标题列表应为80章"

problems = []
chapters = []  # (n, title_line, body_text)

for n in range(1, 81):
    fn = os.path.join(BASE, "chapter_%02d.txt" % n)
    if not os.path.exists(fn):
        problems.append("缺少文件: %s" % fn)
        continue
    with open(fn, "r", encoding="utf-8-sig", newline="") as f:
        raw = f.read()
    if not raw.strip():
        problems.append("空文件: %s" % fn)
        continue
    lines = raw.splitlines()
    # 去掉文件开头可能的空行，找到标题行
    idx = 0
    while idx < len(lines) and not lines[idx].strip():
        idx += 1
    if idx >= len(lines):
        problems.append("无内容: %s" % fn)
        continue
    title_line = lines[idx].strip()
    expected = "第%d章，%s" % (n, TITLES[n - 1])
    if title_line != expected:
        problems.append("首行标题不符: %s -> %r (期望 %r)" % (fn, title_line, expected))
    body = "\n".join(lines[idx + 1:]).strip("\n")
    body_len = len(body.replace("\n", "").replace(" ", ""))
    if body_len < 300:
        problems.append("正文过短(疑似截断): %s 正文长度=%d" % (fn, body_len))
    chapters.append((n, title_line, body))

if not chapters:
    print("ERROR: 没有任何章节可合并")
    sys.exit(1)

# 组装 Markdown
header = [
    "# 《我不是戏神》（起点正式名《我不是戏疯子，我是真能穿剧本》）",
    "",
    "- 作者：柳白羊/三九音域",
    "- 来源：起点中文网免费章节",
    "- 书籍页：https://www.qidian.com/book/1042702471/",
    "- 说明：本文件仅包含第 1-80 章免费试读内容；第 81 章起为 VIP 章节，未收录。",
    "",
    "## 章节目录",
    "",
]
for n in range(1, 81):
    header.append("%d. 第%d章，%s" % (n, n, TITLES[n - 1]))
header.append("")
header.append("---")
header.append("")

parts = list(header)
for n, title_line, body in chapters:
    parts.append("## " + title_line)
    parts.append("")
    parts.append(body)
    parts.append("")
    parts.append("---")
    parts.append("")

md_content = "\n".join(parts) + "\n"

with open(MD_PATH, "w", encoding="utf-8", newline="\n") as f:
    f.write(md_content)

print("已生成: %s" % MD_PATH)
print("合并章节数: %d" % len(chapters))
print("Markdown 字符数: %d" % len(md_content))
if problems:
    print("\n发现 %d 个问题:" % len(problems))
    for p in problems:
        print(" - " + p)
    sys.exit(2)
else:
    print("校验通过：80 章文件齐全、标题一致、正文长度正常。")
