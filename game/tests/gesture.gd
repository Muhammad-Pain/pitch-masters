extends Node
## Smoke test for PES-style buttons and finger gestures (tap pass, swipe shot, slide, team press).
func _ready() -> void:
	var app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	app.show_screen("match", {"mode": "quick", "minutes": 4, "seed": 3})
	var m = app.get_current()
	var f0 = FileAccess.open("/tmp/gest.log", FileAccess.WRITE)
	f0.store_line("started")
	f0.close()
	for i in 150: await get_tree().physics_frame
	var acts = ["tap:10:0", "swipe:1:0.1", "slide", "tpress_on", "tpress_off", "tap:-5:8", "swipe:-1:0", "pass", "through", "lob", "shoot"]
	for k in 12:
		for a in acts:
			var f = FileAccess.open("/tmp/gest.log", FileAccess.WRITE)
			f.store_line("act %d %s %s" % [k, a, str(Time.get_ticks_msec())])
			f.close()
			m.local_action(a, 0.9)
			for i in 20: await get_tree().physics_frame
	print("GESTURE TEST OK clock=", str(Time.get_ticks_msec()))
	get_tree().quit()
