extends Node
## AI vs AI matches through the C++ engine. Run: godot --headless --fixed-fps 60 res://tests/sim.tscn -- 3
var app
var n := 2
var done := 0
var goals := 0

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	if a.size() > 0: n = int(a[0])
	app = PMApp.new()
	add_child(app)
	await get_tree().process_frame
	_next()

func _next() -> void:
	if done >= n:
		print("SIM TOTAL matches=%d goals=%d" % [done, goals])
		get_tree().quit()
		return
	app.show_screen("match", {"mode": "sim", "minutes": 4, "seed": done + 1})
	var m = app.get_current()
	m.sim_finished.connect(_on_done)

func _on_done(r: Dictionary) -> void:
	done += 1
	goals += r.score[0] + r.score[1]
	print("SIM %d: %d-%d shots %s poss %s dbg %s" % [done, r.score[0], r.score[1], str(r.shots), str(r.poss), str(app.get_current().get_dbg())])
	call_deferred("_next")

var _f := 0
func _physics_process(_d: float) -> void:
	_f += 1
	if _f % 600 == 0 and app and app.get_current() and app.get_current().get_class() == "PMMatch":
		var m = app.get_current()
		print("  f=%d clock=%.1f state=%d score=%s ball=%s" % [_f, m.get_clock(), m.get_state(), str(m.get_score()), str(m.get_ball_pos())])
