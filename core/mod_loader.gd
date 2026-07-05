extends RefCounted

const MODS_ROOT := "res://mods"
const MOD_LOG_PREFIX := "[ModLoader]"
const REQUIRED_MOD_FIELDS := ["id", "name", "version", "author", "supported_game_version", "enabled"]
const DATA_FILES := {
	"upgrades": "upgrades.json",
	"enemies": "enemies.json",
	"weapons": "weapons.json",
	"items": "items.json",
	"stages": "stages.json"
}
const ASSET_FIELDS := ["icon", "scene_path", "texture", "image", "sound", "audio", "music"]
const BLOCKED_ASSET_EXTENSIONS := ["gd", "cs", "dll", "so", "dylib", "exe", "bat", "cmd", "sh"]
const REQUIRED_ENTRY_FIELDS := {
	"upgrades": ["base_text", "base_val"],
	"enemies": ["health", "speed", "scale", "color", "damage", "exp", "base_pitch", "scene_path"],
	"weapons": ["display_name", "scene_path", "icon", "max_level", "levels"],
	"items": ["name", "icon", "type", "value", "rarity", "desc"],
	"stages": []
}

func load_mods(game_version: String) -> Array:
	var mod_dirs := _get_mod_directories()
	var candidates: Array = []
	var candidate_ids: Dictionary = {}
	var loaded_mods: Array = []

	for mod_dir in mod_dirs:
		var mod_json_path := MODS_ROOT.path_join(mod_dir).path_join("mod.json")
		var mod_result := _read_json_file(mod_json_path)
		if not mod_result["ok"]:
			_log_error("Skipping %s: %s" % [mod_dir, mod_result["error"]])
			continue

		var mod_data = mod_result["data"]
		var validation := _validate_mod_metadata(mod_data, mod_dir, game_version)
		if not validation["ok"]:
			_log_error("Skipping %s: %s" % [mod_dir, validation["error"]])
			continue

		if not validation["enabled"]:
			_log("Skipping disabled mod %s" % validation["id"])
			continue

		if candidate_ids.has(validation["id"]):
			_log_error("Skipping %s: duplicate mod id '%s'" % [mod_dir, validation["id"]])
			continue

		validation["root_path"] = MODS_ROOT.path_join(mod_dir)
		validation["folder"] = mod_dir
		candidate_ids[validation["id"]] = true
		candidates.append(validation)

	for mod_info in candidates:
		var missing_dependencies: Array = []
		for dependency_id in mod_info["dependencies"]:
			if not candidate_ids.has(dependency_id):
				missing_dependencies.append(dependency_id)

		if missing_dependencies.size() > 0:
			_log_error("Skipping %s: missing dependencies %s" % [mod_info["id"], _join_strings(missing_dependencies)])
			continue

		loaded_mods.append(mod_info)
		_log("Loaded %s v%s by %s" % [mod_info["name"], mod_info["version"], mod_info["author"]])

	return loaded_mods

func merge_mod_data(mods: Array, targets: Dictionary, fallback_assets: Dictionary) -> void:
	var owners := _build_base_owners(targets)

	for mod_info in mods:
		for category in DATA_FILES:
			if not targets.has(category):
				continue

			var data_path: String = mod_info["root_path"].path_join(DATA_FILES[category])
			if not FileAccess.file_exists(data_path):
				continue

			var data_result := _read_json_file(data_path)
			if not data_result["ok"]:
				_log_error("%s: skipping %s: %s" % [mod_info["id"], DATA_FILES[category], data_result["error"]])
				continue

			var entries := _normalize_entries(category, data_result["data"], mod_info["id"])
			if entries.is_empty():
				continue

			_merge_entries(category, entries, mod_info, targets[category], owners[category], fallback_assets.get(category, {}))

func _get_mod_directories() -> Array:
	var mod_dirs: Array = []
	var dir := DirAccess.open(MODS_ROOT)
	if dir == null:
		_log("No mods folder found at %s" % MODS_ROOT)
		return mod_dirs

	dir.list_dir_begin()
	var entry := dir.get_next()
	while entry != "":
		if dir.current_is_dir() and not entry.begins_with("."):
			mod_dirs.append(entry)
		entry = dir.get_next()
	dir.list_dir_end()
	mod_dirs.sort()
	return mod_dirs

func _validate_mod_metadata(data, folder: String, game_version: String) -> Dictionary:
	if typeof(data) != TYPE_DICTIONARY:
		return {"ok": false, "error": "mod.json must contain an object"}

	for field in REQUIRED_MOD_FIELDS:
		if not data.has(field):
			return {"ok": false, "error": "mod.json is missing '%s'" % field}

	var mod_id := str(data["id"]).strip_edges()
	if not _is_valid_id(mod_id):
		return {"ok": false, "error": "mod id must use letters, numbers, '.', '_' or '-'"}

	var name := str(data["name"]).strip_edges()
	var version := str(data["version"]).strip_edges()
	var author := str(data["author"]).strip_edges()
	if name == "" or version == "" or author == "":
		return {"ok": false, "error": "name, version and author must be non-empty strings"}

	if typeof(data["enabled"]) != TYPE_BOOL:
		return {"ok": false, "error": "enabled must be true or false"}

	if not _supports_game_version(data["supported_game_version"], game_version):
		return {"ok": false, "error": "unsupported game version '%s' for game '%s'" % [str(data["supported_game_version"]), game_version]}

	var dependencies: Array = []
	if data.has("dependencies"):
		if typeof(data["dependencies"]) != TYPE_ARRAY:
			return {"ok": false, "error": "dependencies must be an array of mod ids"}
		for dependency in data["dependencies"]:
			var dependency_id := str(dependency).strip_edges()
			if not _is_valid_id(dependency_id):
				return {"ok": false, "error": "dependency id '%s' is invalid" % dependency_id}
			dependencies.append(dependency_id)

	return {
		"ok": true,
		"id": mod_id,
		"name": name,
		"version": version,
		"author": author,
		"supported_game_version": str(data["supported_game_version"]),
		"enabled": bool(data["enabled"]),
		"dependencies": dependencies,
		"folder": folder
	}

func _supports_game_version(value, game_version: String) -> bool:
	if typeof(value) == TYPE_STRING:
		var supported := str(value).strip_edges()
		return supported == "*" or supported == game_version

	if typeof(value) == TYPE_ARRAY:
		for supported in value:
			var version := str(supported).strip_edges()
			if version == "*" or version == game_version:
				return true

	return false

func _is_valid_id(value: String) -> bool:
	if value == "":
		return false

	for i in range(value.length()):
		var code := value.unicode_at(i)
		var character := value.substr(i, 1)
		var is_number := code >= 48 and code <= 57
		var is_upper := code >= 65 and code <= 90
		var is_lower := code >= 97 and code <= 122
		var is_separator := character == "_" or character == "-" or character == "."
		if not (is_number or is_upper or is_lower or is_separator):
			return false

	return true

func _join_strings(values: Array) -> String:
	var text := ""
	for value in values:
		if text != "":
			text += ", "
		text += str(value)
	return text

func _read_json_file(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		return {"ok": false, "error": "file not found"}

	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {"ok": false, "error": "could not open file"}

	var parser := JSON.new()
	var error := parser.parse(file.get_as_text())
	if error != OK:
		return {"ok": false, "error": "bad JSON at line %d: %s" % [parser.get_error_line(), parser.get_error_message()]}

	return {"ok": true, "data": parser.data}

func _normalize_entries(category: String, data, mod_id: String) -> Array:
	var entries: Array = []
	var seen_ids: Dictionary = {}

	if typeof(data) == TYPE_DICTIONARY:
		for entry_id in data:
			var value = data[entry_id]
			if typeof(value) != TYPE_DICTIONARY:
				_log_error("%s: %s entry '%s' must be an object" % [mod_id, category, str(entry_id)])
				continue

			var entry: Dictionary = value.duplicate(true)
			var stable_id := str(entry_id).strip_edges()
			if not _is_valid_id(stable_id):
				_log_error("%s: %s entry key '%s' is not a valid id" % [mod_id, category, stable_id])
				continue
			if entry.has("id") and str(entry["id"]).strip_edges() != stable_id:
				_log_error("%s: %s entry key '%s' does not match id '%s'" % [mod_id, category, stable_id, str(entry["id"])])
				continue
			if seen_ids.has(stable_id):
				_log_error("%s: duplicate %s id '%s'; later entry wins" % [mod_id, category, stable_id])
			entry["id"] = stable_id
			if not _entry_has_required_fields(category, entry, mod_id):
				continue
			seen_ids[stable_id] = true
			entries.append(entry)
		return entries

	if typeof(data) == TYPE_ARRAY:
		for value in data:
			if typeof(value) != TYPE_DICTIONARY:
				_log_error("%s: %s array entries must be objects" % [mod_id, category])
				continue

			var entry: Dictionary = value.duplicate(true)
			var stable_id: String = str(entry.get("id", "")).strip_edges()
			if not _is_valid_id(stable_id):
				_log_error("%s: %s entry is missing a valid id" % [mod_id, category])
				continue
			if seen_ids.has(stable_id):
				_log_error("%s: duplicate %s id '%s'; later entry wins" % [mod_id, category, stable_id])
			entry["id"] = stable_id
			if not _entry_has_required_fields(category, entry, mod_id):
				continue
			seen_ids[stable_id] = true
			entries.append(entry)
		return entries

	_log_error("%s: %s data must be an object or array" % [mod_id, category])
	return entries

func _entry_has_required_fields(category: String, entry: Dictionary, mod_id: String) -> bool:
	for field in REQUIRED_ENTRY_FIELDS.get(category, []):
		if not entry.has(field):
			_log_error("%s: skipping %s '%s': missing '%s'" % [mod_id, category, str(entry.get("id", "")), field])
			return false
	return true

func _merge_entries(category: String, entries: Array, mod_info: Dictionary, target, owners: Dictionary, fallback_assets: Dictionary) -> void:
	if typeof(target) == TYPE_ARRAY:
		_merge_array_entries(category, entries, mod_info, target, owners, fallback_assets)
	elif typeof(target) == TYPE_DICTIONARY:
		_merge_dictionary_entries(category, entries, mod_info, target, owners, fallback_assets)
	else:
		_log_error("%s: cannot merge unsupported target for %s" % [mod_info["id"], category])

func _merge_array_entries(category: String, entries: Array, mod_info: Dictionary, target: Array, owners: Dictionary, fallback_assets: Dictionary) -> void:
	var index_by_id := _array_index_by_id(target)

	for raw_entry in entries:
		var entry: Dictionary = raw_entry.duplicate(true)
		var entry_id := str(entry["id"])
		var existing_entry := {}

		if index_by_id.has(entry_id):
			existing_entry = target[index_by_id[entry_id]]

		_validate_asset_fields(category, entry, mod_info, existing_entry, fallback_assets)

		if index_by_id.has(entry_id):
			_log_override(category, entry_id, mod_info["id"], owners.get(entry_id, "base"))
			target[index_by_id[entry_id]] = entry
		else:
			target.append(entry)
			index_by_id[entry_id] = target.size() - 1

		owners[entry_id] = mod_info["id"]

func _merge_dictionary_entries(category: String, entries: Array, mod_info: Dictionary, target: Dictionary, owners: Dictionary, fallback_assets: Dictionary) -> void:
	for raw_entry in entries:
		var entry: Dictionary = raw_entry.duplicate(true)
		var entry_id := str(entry["id"])
		var existing_entry: Dictionary = target.get(entry_id, {})

		_validate_asset_fields(category, entry, mod_info, existing_entry, fallback_assets)

		if target.has(entry_id):
			_log_override(category, entry_id, mod_info["id"], owners.get(entry_id, "base"))

		target[entry_id] = entry
		owners[entry_id] = mod_info["id"]

func _array_index_by_id(values: Array) -> Dictionary:
	var index_by_id: Dictionary = {}
	for i in range(values.size()):
		var value = values[i]
		if typeof(value) == TYPE_DICTIONARY and value.has("id"):
			index_by_id[str(value["id"])] = i
	return index_by_id

func _build_base_owners(targets: Dictionary) -> Dictionary:
	var owners: Dictionary = {}
	for category in targets:
		owners[category] = {}
		var target = targets[category]
		if typeof(target) == TYPE_ARRAY:
			for value in target:
				if typeof(value) == TYPE_DICTIONARY and value.has("id"):
					owners[category][str(value["id"])] = "base"
		elif typeof(target) == TYPE_DICTIONARY:
			for entry_id in target:
				owners[category][str(entry_id)] = "base"
	return owners

func _validate_asset_fields(category: String, entry: Dictionary, mod_info: Dictionary, existing_entry: Dictionary, fallback_assets: Dictionary) -> void:
	for field in ASSET_FIELDS:
		if not entry.has(field):
			continue

		var fallback: String = str(existing_entry.get(field, fallback_assets.get(field, "")))
		var resolved := _resolve_asset_path(str(entry[field]), field, mod_info["root_path"], fallback, "%s:%s.%s" % [category, str(entry["id"]), field])
		if resolved == "":
			entry.erase(field)
		else:
			entry[field] = resolved

func _resolve_asset_path(value: String, field: String, mod_root: String, fallback: String, context: String) -> String:
	var path := value.strip_edges()
	if path == "":
		_log_error("%s has an empty asset path; using fallback" % context)
		return fallback

	if path.contains(".."):
		_log_error("%s uses an unsafe asset path '%s'; using fallback" % [context, path])
		return fallback

	if field == "scene_path":
		return _resolve_scene_path(path, fallback, context)

	var resolved := _resolve_mod_asset_path(path, mod_root)
	if resolved == "":
		_log_error("%s must point inside this mod's assets/ folder; using fallback" % context)
		return fallback

	if _is_blocked_asset_extension(resolved):
		_log_error("%s points to executable code '%s'; using fallback" % [context, resolved])
		return fallback

	if not _resource_exists(resolved):
		_log_error("%s missing asset '%s'; using fallback" % [context, resolved])
		return fallback

	return resolved

func _resolve_scene_path(path: String, fallback: String, context: String) -> String:
	if not path.begins_with("res://") or path.begins_with(MODS_ROOT + "/"):
		_log_error("%s must use an existing base-game scene path; using fallback" % context)
		return fallback

	if _is_blocked_asset_extension(path):
		_log_error("%s points to executable code '%s'; using fallback" % [context, path])
		return fallback

	if not ResourceLoader.exists(path):
		_log_error("%s missing scene '%s'; using fallback" % [context, path])
		return fallback

	return path

func _resolve_mod_asset_path(path: String, mod_root: String) -> String:
	if path.begins_with("assets/"):
		return mod_root.path_join(path)
	if path.begins_with("./assets/"):
		return mod_root.path_join(path.substr(2))
	return ""

func _is_blocked_asset_extension(path: String) -> bool:
	var extension := path.get_extension().to_lower()
	return extension in BLOCKED_ASSET_EXTENSIONS

func _resource_exists(path: String) -> bool:
	return ResourceLoader.exists(path) or FileAccess.file_exists(path)

func _log_override(category: String, entry_id: String, mod_id: String, previous_owner: String) -> void:
	if previous_owner == "base":
		_log("%s overrides base %s '%s'" % [mod_id, category, entry_id])
	else:
		_log("Conflict on %s '%s': %s overrides %s" % [category, entry_id, mod_id, previous_owner])

func _log(message: String) -> void:
	print("%s %s" % [MOD_LOG_PREFIX, message])

func _log_error(message: String) -> void:
	push_warning("%s %s" % [MOD_LOG_PREFIX, message])
