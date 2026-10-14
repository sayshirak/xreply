# X互评助手 (xreply)

独立于 `xfollowing`（互关）的互评工具。

## 构建

```bat
build_win.bat
```

输出：`D:\program\xreply\xreply\xreply.exe`

## 运行数据（与互关隔离）

- `xreply\userdata\` — 登录态
- `xreply\data\following.json` — 蓝V关注缓存
- `xreply\data\replied.json` — 每日已回复
- `xreply\settings.ini` — 关键词/文案/间隔等
