extends Node
## Builds a custom .pmpatch (json + PNG logo), installs and activates it, plus the bundled example.
var app

func _make_patch(path: String) -> void:
	var zp := ZIPPacker.new()
	zp.open(path)
	var teams := []
	for i in 9:
		var players := []
		for k in 11:
			players.append({"name": "Игрок %d-%d" % [i, k], "pos": ["GK","CB","CB","LB","RB","CMF","CMF","DMF","LWF","RWF","CF"][k], "ovr": 70 + i, "num": k + 1})
		teams.append({"id": "t%d" % i, "name": "Тестовый клуб %d" % i, "short": "Т%d" % i, "shirt": "#%02x3366" % (i * 25), "shorts": "#ffffff", "strength": 70 + i, "logo": "logos/t%d.png" % i, "players": players})
	var j := {"name": "Тест-патч", "author": "QA", "version": "0.9", "league_name": "Тест-лига", "boards": ["ТЕСТ 1", "ТЕСТ 2"], "teams": teams,
		"my_team": {"name": "Мой Тест", "short": "МТС", "shirt": "#ff0000", "shorts": "#000000"}, "menu_background": "images/bg.png"}
	zp.start_file("mypatch/patch.json")
	zp.write_file(JSON.stringify(j).to_utf8_buffer())
	zp.close_file()
	var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.2, 0.6, 1.0))
	for i in 9:
		zp.start_file("mypatch/logos/t%d.png" % i)
		zp.write_file(img.save_png_to_buffer())
		zp.close_file()
	zp.start_file("mypatch/images/bg.png")
	zp.write_file(img.save_png_to_buffer())
	zp.close_file()
	zp.start_file("mypatch/../evil.png")
	zp.write_file(PackedByteArray([1, 2, 3]))
	zp.close_file()
	zp.close()

func _ready() -> void:
	app = PMApp.new()
	add_child(app)
	for i in 3: await get_tree().process_frame
	var path := "user://qa_test.pmpatch"
	_make_patch(path)
	var id: String = app.debug_install_patch(path)
	print("installed: ", id)
	print("bad file: ", app.debug_install_patch("user://nope.pmpatch"))
	var f := FileAccess.open("user://broken.json", FileAccess.WRITE)
	f.store_string("{not json")
	f.close()
	print("broken json: ", app.debug_install_patch("user://broken.json"))
	print("evil escaped? ", FileAccess.file_exists("user://patches/evil.png"))
	print("activate: '", app.debug_activate_patch(id), "'")
	var info: Dictionary = app.debug_info()
	print("league=", info.league_name, " team=", info.team_name, " boards=", info.boards)
	var with_logo := 0
	for t in info.teams:
		if t.logo != "": with_logo += 1
	print("league teams=", info.teams.size(), " with logos=", with_logo, " first=", info.teams[0])
	var ex: String = app.debug_install_example()
	print("example: ", ex)
	print("patches: ", app.debug_list_patches())
	print("activate example: '", app.debug_activate_patch(ex), "'")
	info = app.debug_info()
	print("league=", info.league_name, " team=", info.team_name)
	# play a short match with patch teams to make sure logos/players load
	app.show_screen("league", {})
	await get_tree().process_frame
	app.show_screen("match", {"mode": "sim", "minutes": 1, "seed": 3})
	await get_tree().process_frame
	var m = app.get_current()
	var r = await m.sim_finished
	print("patch match: ", r.score, " names ", r.names)
	app.show_screen("patches", {})
	for i in 3: await get_tree().process_frame
	print("deactivate: '", app.debug_activate_patch(""), "' league=", app.debug_info().league_name)
	print("PATCH TEST DONE")
	get_tree().quit()
