
#include "transition-table.hpp"
#include "version.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>

#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTableView>
#include <QTableWidget>
#include <QtWidgets/QColorDialog>

#include <fstream>
#include <string>
#include <sstream>
#include <QString>
#include <QDir>
#include <iostream>
#include <QMessageBox>
#include <QInputDialog>
#include <QColor>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("transition-table", "en-US")

using namespace std;

struct transition_info {
	string transition;
	int duration;
};

map<string, map<string, transition_info>> transition_table;

int transition_table_width = 0;
int transition_table_height = 0;
auto ttr = new TransitionTableRefresh();
auto md = new QDialog();

obs_hotkey_pair_id transition_table_hotkey = OBS_INVALID_HOTKEY_PAIR_ID;

int min_width								= 800;		
int min_height								= 400;

QString tt_current_setname		= "";
QString tt_default_setname		= "";
bool tt_ignore_default_set		= false;

bool isactive_import_export		= false;

typedef struct tt_setlist_list_elem {
	obs_hotkey_id hotkey_id;
	string setname;
	obs_data_array_t *transition_array;
	struct tt_setlist_list_elem *next_elem;
} tt_setlist_list_elem_t;

tt_setlist_list_elem_t *setlist_ptr = NULL;

static void load_transition_matrix(obs_data_t *obj)
{
	obs_data_array_t *transitions = obs_data_get_array(obj, "matrix");
	if (!transitions)
		return;
	const size_t count = obs_data_array_count(transitions);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *transition = obs_data_array_item(transitions, i);
		string fromScene = obs_data_get_string(transition, "scene");
		obs_data_array_t *data = obs_data_get_array(transition, "data");
		const size_t transition_count = obs_data_array_count(data);
		for (size_t j = 0; j < transition_count; j++) {
			obs_data_t *transition2 = obs_data_array_item(data, j);
			string toScene = obs_data_get_string(transition2, "to");
			if (!fromScene.empty() && !toScene.empty()) {
				transition_table[fromScene][toScene].transition = obs_data_get_string(transition2, "transition");
				transition_table[fromScene][toScene].duration = obs_data_get_int(transition2, "duration");
			}
			obs_data_release(transition2);
		}
		obs_data_array_release(data);
		obs_data_release(transition);
	}
	obs_data_array_release(transitions);
}


static void load_transitions(obs_data_t *obj)
{
	obs_data_array_t *transitions =
		obs_data_get_array(obj, "transitions");
	if (!transitions)
		return;
	const size_t count =
		obs_data_array_count(transitions);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *transition =
			obs_data_array_item(transitions,
			                    i);
		string fromScene = obs_data_get_string(
			transition, "from_scene");
		string toScene = obs_data_get_string(
			transition, "to_scene");
		string transitionName =
			obs_data_get_string(
				transition,
				"transition");
		const uint32_t duration =
			obs_data_get_int(transition,
			                 "duration");
		transition_table[fromScene][toScene]
			.transition = transitionName;
		transition_table[fromScene][toScene]
			.duration = duration;
		obs_data_release(transition);
	}
	obs_data_array_release(transitions);
}



tt_setlist_list_elem_t *setlist_get_elem(tt_setlist_list_elem_t *list_ptr, string setname)
{
	if (list_ptr == NULL) {
		return NULL;
	}
	while (list_ptr->setname != setname) {
		if (list_ptr->next_elem == NULL) {
			return NULL;
		}
		list_ptr = list_ptr->next_elem;
	}
	return list_ptr;
}


tt_setlist_list_elem_t *setlist_get_elem(tt_setlist_list_elem_t *list_ptr, obs_hotkey_id id)
{
	if (list_ptr == NULL) {
		return NULL;
	}
	while (list_ptr->hotkey_id != id) {

		if (list_ptr->next_elem == NULL) {
			return NULL;
		}
		list_ptr = list_ptr->next_elem;
	}
	return list_ptr;
}

static void clear_transition_overrides()
{
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_data_t *data = obs_source_get_private_settings(scenes.sources.array[i]);
		obs_data_erase(data, "transition");
		obs_data_release(data);
	}
	obs_frontend_source_list_free(&scenes);
}

static void set_transition_overrides()
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	string fromScene = obs_source_get_name(scene);
	obs_source_release(scene);
	auto fs_it = fromScene.empty() ? transition_table.end(): transition_table.find(fromScene);
	auto as_it = transition_table.find("Any");
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		string toScene = obs_source_get_name(scenes.sources.array[i]);
		string transition;
		auto duration = 0;
		if (fs_it != transition_table.end()) {
			auto to_it = fs_it->second.find(toScene);
			if (to_it == fs_it->second.end()) {
				to_it = fs_it->second.find("Any");
			}
			if (to_it != fs_it->second.end()) {
				transition = to_it->second.transition;
				duration = to_it->second.duration;
			}
		}
		if (transition.empty() && as_it != transition_table.end()) {
			auto to_it = as_it->second.find(toScene);
			if (to_it == as_it->second.end()) {
				to_it = as_it->second.find("Any");
			}
			if (to_it != as_it->second.end()) {
				transition = to_it->second.transition;
				duration = to_it->second.duration;
			}
		}
		obs_data_t *data = obs_source_get_private_settings(
			scenes.sources.array[i]);
		if (transition.empty()) {
			obs_data_erase(data, "transition");
		} else {
			obs_data_set_string(data, "transition",
					    transition.c_str());
			obs_data_set_int(data, "transition_duration", duration);
		}
		obs_data_release(data);
	}
	obs_frontend_source_list_free(&scenes);
}

static bool transition_table_enabled = true;

void save_set_function()
{
	tt_setlist_list_elem_t *elem = setlist_get_elem(setlist_ptr, tt_current_setname.toUtf8().constData());
	elem->transition_array = obs_data_array_create();
	for (const auto &it : transition_table) {
		for (const auto &it2 : it.second) {
			obs_data_t *transition = obs_data_create();
			obs_data_set_string(transition, "from_scene", it.first.c_str());
			obs_data_set_string(transition, "to_scene", it2.first.c_str());
			obs_data_set_string(transition, "transition", it2.second.transition.c_str());
			obs_data_set_int(transition, "duration", it2.second.duration);
			obs_data_array_push_back(elem->transition_array, transition);
			obs_data_release(transition);
		}
	}
}

void load_set_function()
{
	transition_table.clear();
	tt_setlist_list_elem_t *elem = setlist_get_elem(setlist_ptr, tt_current_setname.toUtf8().constData());
	if (elem->transition_array) {
		size_t count = obs_data_array_count(elem->transition_array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *transition = obs_data_array_item(elem->transition_array, i);
			string fromScene = obs_data_get_string(transition, "from_scene");
			string toScene = obs_data_get_string(transition, "to_scene");
			string transitionName = obs_data_get_string(transition, "transition");
			const uint32_t duration = obs_data_get_int(transition, "duration");
			transition_table[fromScene][toScene].transition = transitionName;
			transition_table[fromScene][toScene].duration = duration;
			obs_data_release(transition);
		}
	}
	if (transition_table_enabled)
		set_transition_overrides();
}

void hotkey_set_set(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	if (isactive_import_export == false) {
		tt_setlist_list_elem_t *elem = setlist_get_elem(setlist_ptr, id);
		if (elem != NULL) {
			if (QString::fromUtf8(elem->setname) != tt_current_setname) {
				tt_current_setname = QString::fromUtf8(elem->setname);
				load_set_function();
				ttr->signalRefresh();
			}
		}
	}
}

int register_hotkey(string setname)
{
	tt_setlist_list_elem_t *elem = setlist_get_elem(setlist_ptr, setname);
	if (elem == NULL) {
		return -1;
	}
	elem->hotkey_id = obs_hotkey_register_frontend(("transition-table." + setname).c_str(),
		(obs_module_text("TransitionTableSet") + setname + "\"").c_str(), hotkey_set_set, nullptr);
	return 0;
}

int unregister_hotkey(string setname)
{
	tt_setlist_list_elem_t *elem = setlist_get_elem(setlist_ptr, setname);
	if (elem == NULL) {
		return -1;
	}
	obs_hotkey_unregister(elem->hotkey_id);
	return 0;
}

tt_setlist_list_elem_t *setlist_create_elem(void)
{
	tt_setlist_list_elem_t *new_elem = new tt_setlist_list_elem_t;
	if (new_elem == NULL) {
		return NULL;
	}
	new_elem->hotkey_id = 0;
	new_elem->setname = "";
	new_elem->next_elem = NULL;
	new_elem->transition_array = NULL;
	return new_elem;
}

int setlist_append_elem(tt_setlist_list_elem_t **list_ptr, tt_setlist_list_elem_t *new_elem, string setname)
{
	if (list_ptr == NULL) {
		return -2;
	}
	if (new_elem == NULL) {
		return -1;
	}
	new_elem->next_elem = NULL;
	new_elem->setname = setname;
	if (*list_ptr == NULL) {
		*list_ptr = new_elem;
	} else {
		tt_setlist_list_elem_t *current = *list_ptr;
		while (current->next_elem != NULL) {
			current = current->next_elem;
		}
		current->next_elem = new_elem;
	}
	register_hotkey(setname);
	return 0;
}

int setlist_destroy_elem(tt_setlist_list_elem_t **list_ptr, tt_setlist_list_elem_t *elem)
{
	if (elem == NULL) {
		return -1;
	}
	if (list_ptr == NULL || *list_ptr == NULL) {
		return -2;
	}
	if (*list_ptr == elem) {
		unregister_hotkey(elem->setname);
		*list_ptr = elem->next_elem;
	} else {
		tt_setlist_list_elem_t *current = *list_ptr;
		while (current->next_elem != elem) {
			if (current->next_elem == NULL) {
				return -3;
			}
			current = current->next_elem;
		}
		unregister_hotkey(elem->setname);
		current->next_elem = elem->next_elem;
	}
	free(elem);
	return 0;
}

int setlist_destroy_elem(tt_setlist_list_elem_t **listPtr, string setname)
{
	tt_setlist_list_elem_t *elem = setlist_get_elem(*listPtr, setname);
	return setlist_destroy_elem(listPtr, elem);
}

void setlist_destroy_list(tt_setlist_list_elem_t **listPtr)
{
	if (listPtr == NULL || *listPtr == NULL) {
		return;
	}
	tt_setlist_list_elem_t *to_free = *listPtr;
	tt_setlist_list_elem_t *next = NULL;
	do {
		next = to_free->next_elem;
		unregister_hotkey(to_free->setname);
		free(to_free);
		to_free = next;
		*listPtr = next;
	} while (to_free != NULL);
	*listPtr = NULL;
}

static void frontend_save_load(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *transitions = obs_data_array_create();
		obs_data_set_obj(save_data, "transition-table", obj);
		for (const auto &it : transition_table) {
			for (const auto &it2 : it.second) {
				obs_data_t *transition = obs_data_create();
				obs_data_set_string(transition, "from_scene", it.first.c_str());
				obs_data_set_string(transition, "to_scene", it2.first.c_str());
				obs_data_set_string(transition, "transition", it2.second.transition.c_str());
				obs_data_set_int(transition, "duration", it2.second.duration);
				obs_data_array_push_back(transitions, transition);
				obs_data_release(transition);
			}
		}
		obs_data_set_array(obj, "transitions", transitions);
		if (transition_table_width > min_width && transition_table_height > min_height) {
			obs_data_set_int(obj, "dialog_width", transition_table_width);
			obs_data_set_int(obj, "dialog_height", transition_table_height);
		}
		obs_data_array_t *data0 = nullptr;
		obs_data_array_t *data1 = nullptr;
		obs_hotkey_pair_save(transition_table_hotkey, &data0, &data1);
		if (data0) {
			obs_data_set_array(obj, "enable_hotkey", data0);
			obs_data_array_release(data0);
		}
		if (data1) {
			obs_data_set_array(obj, "disable_hotkey", data1);
			obs_data_array_release(data1);
		}		
		obs_data_set_string(obj, "default_set", tt_default_setname.toUtf8().constData());
		obs_data_set_string(obj, "last_set", tt_current_setname.toUtf8().constData());
		obs_data_set_bool(obj, "ignore_default", tt_ignore_default_set);
		obs_data_array_t *setname_array = obs_data_array_create();
		tt_setlist_list_elem_t *to_save = setlist_ptr;
		do {
			obs_data_array_t *data = obs_hotkey_save(to_save->hotkey_id);
			if (data) {
				obs_data_set_array(obj, ("hotkey_set_" + to_save->setname).c_str(), data);
				obs_data_array_release(data);
			}
			obs_data_t *current_set = obs_data_create();
			obs_data_set_string(current_set, "setname", (to_save->setname).c_str());
			obs_data_array_push_back(setname_array, current_set);
			obs_data_set_array(obj, ("table_set_" + to_save->setname).c_str(), to_save->transition_array);
			obs_data_release(current_set);
			to_save = to_save->next_elem;
		} while (to_save != NULL);
		obs_data_set_array(obj, "setname_array", setname_array);
		obs_data_set_obj(save_data, "transition-table", obj);
		obs_data_array_release(transitions);
		obs_data_release(obj);
	} else {
		transition_table.clear();
		obs_data_t *obj = obs_data_get_obj(save_data, "transition-table");
		if (obj) {
			transition_table_width = obs_data_get_int(obj, "dialog_width");
			transition_table_height = obs_data_get_int(obj, "dialog_height");
			obs_data_array_t *eh = obs_data_get_array(obj, "enable_hotkey");
			obs_data_array_t *dh = obs_data_get_array(obj, "disable_hotkey");
			obs_hotkey_pair_load(transition_table_hotkey, eh, dh);
			obs_data_array_release(eh);
			obs_data_array_release(dh);
			obs_data_array_t *setname_array = obs_data_get_array(obj, "setname_array");
			if (setname_array) {
				tt_default_setname = obs_data_get_string(obj, "default_set");
				tt_ignore_default_set = obs_data_get_bool(obj, "ignore_default");
				if (tt_ignore_default_set == true || tt_default_setname == "") {
					tt_current_setname = obs_data_get_string(obj, "last_set");
				} else {
					tt_current_setname = tt_default_setname;
				}
				size_t count = obs_data_array_count(setname_array);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *current_set = obs_data_array_item(setname_array, i);
					string setname = obs_data_get_string(current_set, "setname");
					obs_data_array_t *data = obs_data_get_array(obj, ("hotkey_set_" + setname).c_str());
					tt_setlist_list_elem_t *new_elem = setlist_create_elem();
					setlist_append_elem(&setlist_ptr, new_elem, setname);
					obs_hotkey_load(new_elem->hotkey_id, data);
					new_elem->transition_array = obs_data_get_array(obj, ("table_set_" + setname).c_str());
					if (setname == tt_current_setname.toUtf8().constData()) {
						size_t count_transitions = obs_data_array_count(new_elem->transition_array);
						if (new_elem->transition_array) {
							for (size_t i = 0; i < count_transitions; i++) {
								obs_data_t *transition = obs_data_array_item(new_elem->transition_array, i);
								string fromScene = obs_data_get_string(transition, "from_scene");
								string toScene = obs_data_get_string(transition, "to_scene");
								string transitionName = obs_data_get_string(transition, "transition");
								const uint32_t duration = obs_data_get_int(transition, "duration");
								transition_table[fromScene][toScene].transition = transitionName;
								transition_table[fromScene][toScene].duration = duration;
								obs_data_release(transition);
							}
						}
					}
					obs_data_array_release(data);
					obs_data_release(current_set);
				}
			} else {
				obs_data_array_t *transitions = obs_data_get_array(obj, "transitions");
				if (transitions) {
					size_t count_transitions = obs_data_array_count(transitions);
					for (size_t i = 0; i < count_transitions; i++) {
						obs_data_t *transition = obs_data_array_item(transitions, i);
						string fromScene = obs_data_get_string(transition, "from_scene");
						string toScene = obs_data_get_string(transition, "to_scene");
						string transitionName = obs_data_get_string(transition, "transition");
						const uint32_t duration = obs_data_get_int(transition, "duration");
						transition_table[fromScene][toScene].transition = transitionName;
						transition_table[fromScene][toScene].duration = duration;
						obs_data_release(transition);
					}
				}
				obs_data_array_release(transitions);
				string setname = obs_module_text("InitialSetName");
				obs_data_t *obj = obs_data_create();
				tt_setlist_list_elem_t *new_elem = setlist_create_elem();
				setlist_append_elem(&setlist_ptr, new_elem, setname);
				tt_current_setname = QString::fromUtf8(setname);
				tt_default_setname = "";
				tt_ignore_default_set = false;
				save_set_function();
			}
			obs_data_release(obj);
		} else {
			obj = obs_data_get_obj(save_data, "obs-transition-matrix");
			if (obj) {
				load_transition_matrix(obj);
				obs_data_release(obj);
			}
			obs_frontend_source_list scenes = {};
			obs_frontend_get_scenes(&scenes);
			for (size_t i = 0; i < scenes.sources.num; i++) {
				obs_data_t *data = obs_source_get_private_settings(scenes.sources.array[i]);
				string transitionName = obs_data_get_string(data, "transition");
				string sceneName = obs_source_get_name(scenes.sources.array[i]);
				if (!transitionName.empty()) {
					transition_table["Any"][sceneName].transition = transitionName;
					transition_table["Any"][sceneName].duration = obs_data_get_int(data, "transition_duration");
				}
				obs_data_release(data);
			}
			obs_frontend_source_list_free(&scenes);
		}
	}
}

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		if (transition_table_enabled)
			set_transition_overrides();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP || event == OBS_FRONTEND_EVENT_EXIT) {
		transition_table.clear();
	}
}

static void source_rename(void *data, calldata_t *call_data)
{
	UNUSED_PARAMETER(data);
	string new_name = calldata_string(call_data, "new_name");
	string prev_name = calldata_string(call_data, "prev_name");
	auto it = transition_table.find(prev_name);
	if (it != transition_table.end()) {
		transition_table[new_name] = it->second;
		transition_table.erase(it);
	}
	for (const auto &it : transition_table) {
		auto it2 = it.second.find(prev_name);
		if (it2 != it.second.end()) {
			transition_table[it.first][new_name] = it2->second;
			transition_table[it.first].erase(it2);
		}
	}
}

bool enable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!transition_table_enabled && pressed) {
		transition_table_enabled = true;
		set_transition_overrides();
		return true;
	}
	return false;
}

bool disable_hotkey(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (transition_table_enabled && pressed) {
		transition_table_enabled = false;
		clear_transition_overrides();
		return true;
	}
	return false;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Transition Table] loaded version %s", PROJECT_VERSION);
	auto *action = (QAction *)obs_frontend_add_tools_menu_qaction(obs_module_text("TransitionTable"));

	auto cb = [] {
		obs_frontend_push_ui_translation(obs_module_get_string);
		auto ttd = new TransitionTableDialog((QMainWindow *)obs_frontend_get_main_window());
		QObject::connect(ttr, &TransitionTableRefresh::sigRefresh, ttd, &TransitionTableDialog::RefreshTable);
		QObject::connect(ttr, &TransitionTableRefresh::sigRefresh, ttd, &TransitionTableDialog::RefreshMatrix);
		ttd->setAttribute(Qt::WA_DeleteOnClose);
		ttd->show();
		obs_frontend_pop_ui_translation();
	};

	QAction::connect(action, &QAction::triggered, cb);

	obs_frontend_add_save_callback(frontend_save_load, nullptr);
	obs_frontend_add_event_callback(frontend_event, nullptr);
	signal_handler_connect(obs_get_signal_handler(), "source_rename", source_rename, nullptr);

	transition_table_hotkey = obs_hotkey_pair_register_frontend(
		"transition-table.enable", obs_module_text("TransitionTable.Enable"),
		"transition-table.disable", obs_module_text("TransitionTable.Disable"),
		enable_hotkey, disable_hotkey, nullptr, nullptr);

	return true;
}

void obs_module_unload(void)
{
	obs_hotkey_pair_unregister(transition_table_hotkey);
	obs_frontend_remove_save_callback(frontend_save_load, nullptr);
	setlist_destroy_list(&setlist_ptr);
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename", source_rename, nullptr);
	transition_table.clear();
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("TransitionTable");
}

TransitionTableDialog::TransitionTableDialog(QMainWindow *parent)
	: QDialog(parent)
{
	obs_frontend_get_scenes(&scenes);
	obs_frontend_get_transitions(&transitions);

	int idx = 0;
	mainLayout = new QGridLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	QLabel *label = new QLabel(obs_module_text("FromScene"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("ToScene"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("Transition"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("Duration"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	QCheckBox *checkbox = new QCheckBox;
	mainLayout->addWidget(checkbox, 0, idx++, Qt::AlignCenter);
	mainLayout->setColumnStretch(0, 1);
	mainLayout->setColumnStretch(1, 1);
	mainLayout->setColumnStretch(2, 1);

	connect(checkbox, &QCheckBox::stateChanged, [this]() { SelectAllChanged(); });

	idx = 0;
	fromCombo = new QComboBox();
	fromCombo->setEditable(true);
	auto *completer = fromCombo->completer();
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	fromCombo->addItem("", QByteArray(""));
	fromCombo->addItem(obs_module_text("Any"), QByteArray("Any"));
	mainLayout->addWidget(fromCombo, 1, idx++);
	toCombo = new QComboBox();
	toCombo->setEditable(true);
	completer = toCombo->completer();
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	toCombo->addItem("", QByteArray(""));
	toCombo->addItem(obs_module_text("Any"), QByteArray("Any"));
	mainLayout->addWidget(toCombo, 1, idx++);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		string sceneName = obs_source_get_name(scenes.sources.array[i]);
		fromCombo->addItem(QString::fromUtf8(sceneName.c_str()), QByteArray(sceneName.c_str()));
		toCombo->addItem(QString::fromUtf8(sceneName.c_str()), QByteArray(sceneName.c_str()));
	}

	connect(fromCombo, SIGNAL(editTextChanged(const QString &)), SLOT(RefreshTable()));
	connect(toCombo, SIGNAL(editTextChanged(const QString &)), SLOT(RefreshTable()));

	transitionCombo = new QComboBox();
	for (size_t i = 0; i < transitions.sources.num; i++) {
		string trName = obs_source_get_name(transitions.sources.array[i]);
		transitionCombo->addItem(QString::fromUtf8(trName.c_str()), QByteArray(trName.c_str()));
	}
	mainLayout->addWidget(transitionCombo, 1, idx++);
	durationSpin = new QSpinBox;
	durationSpin->setMinimum(50);
	durationSpin->setMaximum(20000);
	durationSpin->setSingleStep(50);
	durationSpin->setValue(500);
	durationSpin->setSuffix("ms");
	mainLayout->addWidget(durationSpin, 1, idx++);
	QPushButton *addButton = new QPushButton(obs_module_text("Set"));
	connect(addButton, &QPushButton::clicked, [this]() { AddClicked(); });
	mainLayout->addWidget(addButton, 1, idx++, Qt::AlignCenter);

	QWidget *controlArea = new QWidget;
	controlArea->setLayout(mainLayout);
	controlArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(controlArea);
	QWidget *widget = new QWidget;
	widget->setLayout(vlayout);
	widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(widget);
	scrollArea->setWidgetResizable(true);


	QPushButton *closeButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Close")));
	QPushButton *exportButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Export")));
	QPushButton *matrixButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Matrix")));
	QPushButton *importButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Import")));
	QPushButton *deleteButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Delete")));

	QHBoxLayout *bottomLayout = new QHBoxLayout;
	bottomLayout->addWidget(
		new QLabel(
			"<a href=\"https://obsproject.com/forum/resources/transition-table.1174/\">Transition Table</a> (" PROJECT_VERSION
			") by <a href=\"https://www.exeldro.com\">Exeldro</a>"),
		0, Qt::AlignLeft);
	bottomLayout->addWidget(exportButton, 0, Qt::AlignRight);
	bottomLayout->addWidget(importButton, 0, Qt::AlignRight);
	bottomLayout->addWidget(matrixButton, 0, Qt::AlignRight);
	bottomLayout->addWidget(deleteButton, 0, Qt::AlignRight);
	bottomLayout->addWidget(closeButton, 0, Qt::AlignRight);


	QPushButton *duplicateSetButton = new QPushButton(obs_module_text("DuplicateSet"));
	connect(duplicateSetButton, &QPushButton::clicked, [this]() { DuplicateSetClicked(); });
	QPushButton *newSetButton = new QPushButton(obs_module_text("NewSet"));
	connect(newSetButton, &QPushButton::clicked, [this]() { NewSetClicked(); });
	QPushButton *renameSetButton = new QPushButton(obs_module_text("RenameSet"));
	connect(renameSetButton, &QPushButton::clicked, [this]() { RenameSetClicked(); });
	QPushButton *deleteSetButton = new QPushButton(obs_module_text("DeleteSet"));
	connect(deleteSetButton, &QPushButton::clicked, [this]() { DeleteSetClicked(); });
	QPushButton *setDefaultSetButton = new QPushButton(obs_module_text("SetDefault"));
	connect(setDefaultSetButton, &QPushButton::clicked, [this]() { SetDefaultSetClicked(); });

	ignoreDefaultCheckBox = new QCheckBox(obs_module_text("IgnoreDefault"));
	connect(ignoreDefaultCheckBox, &QCheckBox::stateChanged, [this]() { IgnoreDefaultChanged(); });

	selectSetCombo = new QComboBox();
	selectSetCombo->setEditable(false);
	selectSetCombo->view()->setMinimumWidth(300);
	tt_setlist_list_elem_t *next = setlist_ptr;
	do {
		selectSetCombo->addItem(QString::fromUtf8(next->setname));
		next = next->next_elem;
	} while (next != NULL);
	connect(selectSetCombo, &QComboBox::currentTextChanged, [this]() { SelectSetChanged(); });

	RefreshTable();

	newLayout = new QGridLayout;
	newLayout->setColumnStretch(0, 1);
	newLayout->setColumnMinimumWidth(1, 140);
	newLayout->setColumnMinimumWidth(2, 140);
	newLayout->setColumnMinimumWidth(3, 140);
	newLayout->setColumnMinimumWidth(4, 140);
	newLayout->addWidget(selectSetCombo, 0, 0);
	newLayout->addWidget(duplicateSetButton, 0, 1);
	newLayout->addWidget(newSetButton, 0, 2);
	newLayout->addWidget(renameSetButton, 0, 3);
	newLayout->addWidget(deleteSetButton, 0, 4);
	newLayout->addWidget(setDefaultSetButton, 1, 4);
	newLayout->addWidget(ignoreDefaultCheckBox, 1, 3);

	bottomLayout->setStretch(0, 1);
	connect(deleteButton, &QPushButton::clicked,
		[this]() { DeleteClicked(); });
	connect(closeButton, &QPushButton::clicked, [this]() { close(); });
	connect(exportButton, &QPushButton::clicked, [this]() {
		isactive_import_export = true;
		const QString fileName = QFileDialog::getSaveFileName(
			nullptr,
			QString::fromUtf8(
				obs_module_text("SaveTransitionTable")),
			tt_current_setname, "JSON File (*.json)");
		if (fileName.isEmpty()) {
			isactive_import_export = false;
			return;
		}
		const auto fu = fileName.toUtf8();

		const auto transitions_array = obs_data_array_create();
		bool selection = false;
		for (auto row = 2; row < mainLayout->rowCount(); row++) {
			auto *item = mainLayout->itemAtPosition(row, 4);
			if (!item)
				continue;
			auto *checkBox =
				dynamic_cast<QCheckBox *>(item->widget());
			if (!checkBox || !checkBox->isChecked())
				continue;

			item = mainLayout->itemAtPosition(row, 0);
			auto *label = dynamic_cast<QLabel *>(item->widget());
			if (!label)
				continue;
			string fromScene = label->text().toUtf8().constData();
			if (fromScene == obs_module_text("Any"))
				fromScene = "Any";
			auto fs_it = transition_table.find(fromScene);
			if (fs_it == transition_table.end())
				continue;
			item = mainLayout->itemAtPosition(row, 1);
			label = dynamic_cast<QLabel *>(item->widget());
			if (!label)
				continue;
			string toScene = label->text().toUtf8().constData();
			if (toScene == obs_module_text("Any"))
				toScene = "Any";
			auto ts_it = fs_it->second.find(toScene);
			if (ts_it == fs_it->second.end())
				continue;
			selection = true;
			obs_data_t *transition = obs_data_create();
			obs_data_set_string(transition, "from_scene",
					    fromScene.c_str());
			obs_data_set_string(transition, "to_scene",
					    toScene.c_str());
			obs_data_set_string(transition, "transition",
					    ts_it->second.transition.c_str());
			obs_data_set_int(transition, "duration",
					 ts_it->second.duration);
			obs_data_array_push_back(transitions_array, transition);
			obs_data_release(transition);
		}
		if (!selection) {
			for (const auto &it : transition_table) {
				for (const auto &it2 : it.second) {
					obs_data_t *transition =
						obs_data_create();
					obs_data_set_string(transition,
							    "from_scene",
							    it.first.c_str());
					obs_data_set_string(transition,
							    "to_scene",
							    it2.first.c_str());
					obs_data_set_string(
						transition, "transition",
						it2.second.transition.c_str());
					obs_data_set_int(transition, "duration",
							 it2.second.duration);
					obs_data_array_push_back(
						transitions_array, transition);
					obs_data_release(transition);
				}
			}
		}
		obs_data_t *data = obs_data_create();
		obs_data_set_array(data, "transitions", transitions_array);
		obs_data_array_release(transitions_array);
		obs_data_save_json(data, fu.constData());
		obs_data_release(data);
		isactive_import_export = false;
	});
	connect(importButton, &QPushButton::clicked, [this]() {
		isactive_import_export = true;
		const QString fileName = QFileDialog::getOpenFileName(nullptr,
			QString::fromUtf8(obs_module_text("LoadTransitionTable")), QString(), "JSON File (*.json)");
		if (fileName.isEmpty()) {
			isactive_import_export = false;
			return;
		}
		const auto fu = fileName.toUtf8();
		obs_data_t *data =
			obs_data_create_from_json_file(fu.constData());
		load_transitions(data);
		obs_data_release(data);
		RefreshTable();
		if (transition_table_enabled)
			set_transition_overrides();
		save_set_function();
		isactive_import_export = false;
	});
	connect(matrixButton, &QPushButton::clicked, this,
		&TransitionTableDialog::ShowMatrix);

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(11, 11, 11, 11);
	vlayout->addWidget(scrollArea);
	vlayout->addLayout(bottomLayout);
	vlayout->addLayout(newLayout);
	setLayout(vlayout);

	setWindowTitle(obs_module_text("TransitionTable"));
	setSizeGripEnabled(true);

	setMinimumSize(min_width, min_height);
	if (transition_table_width > min_width && transition_table_height > min_height) {
		resize(transition_table_width, transition_table_height);
	}
}

TransitionTableDialog::~TransitionTableDialog()
{
	auto size = this->size();
	transition_table_width = size.width();
	transition_table_height = size.height();
	obs_frontend_source_list_free(&scenes);
	obs_frontend_source_list_free(&transitions);
}

void TransitionTableDialog::AddClicked()
{
	auto fromScene = fromCombo->currentText();
	auto toScene = toCombo->currentText();
	const auto transition = transitionCombo->currentText();
	if (fromScene.isEmpty() || toScene.isEmpty() || transition.isEmpty())
		return;
	if (fromScene == QString::fromUtf8(obs_module_text("Any")))
		fromScene = "Any";
	if (toScene == QString::fromUtf8(obs_module_text("Any")))
		toScene = "Any";

	auto &t = transition_table[fromScene.toUtf8().constData()][toScene.toUtf8().constData()];
	t.transition = transition.toUtf8().constData();
	t.duration = durationSpin->value();
	RefreshTable();
	if (transition_table_enabled)
		set_transition_overrides();
	save_set_function();
}

void TransitionTableDialog::DeleteClicked()
{
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		auto *item = mainLayout->itemAtPosition(row, 4);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox || !checkBox->isChecked())
			continue;

		item = mainLayout->itemAtPosition(row, 0);
		auto *label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		string fromScene = label->text().toUtf8().constData();
		if (fromScene == obs_module_text("Any"))
			fromScene = "Any";
		auto fs_it = transition_table.find(fromScene);
		if (fs_it == transition_table.end())
			continue;
		item = mainLayout->itemAtPosition(row, 1);
		label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		string toScene = label->text().toUtf8().constData();
		if (toScene == obs_module_text("Any"))
			toScene = "Any";
		auto ts_it = fs_it->second.find(toScene);
		if (ts_it == fs_it->second.end())
			continue;
		fs_it->second.erase(ts_it);
	}
	RefreshTable();
	save_set_function();
}

void TransitionTableDialog::SelectAllChanged()
{
	auto *item = mainLayout->itemAtPosition(0, 4);
	auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
	bool checked = checkBox && checkBox->isChecked();
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		item = mainLayout->itemAtPosition(row, 4);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox)
			continue;
		checkBox->setChecked(checked);
	}
}

void TransitionTableDialog::RefreshTable()
{
	auto fromScene = fromCombo->currentText();
	auto toScene = toCombo->currentText();
	if (fromScene == QString::fromUtf8(obs_module_text("Any")))
		fromScene = "Any";
	if (toScene == QString::fromUtf8(obs_module_text("Any")))
		toScene = "Any";
	for (auto row = mainLayout->rowCount() - 1; row >= 2; row--) {
		for (auto col = mainLayout->columnCount() - 1; col >= 0;
		     col--) {
			auto *item = mainLayout->itemAtPosition(row, col);
			if (item) {
				mainLayout->removeItem(item);
				delete item->widget();
				delete item;
			}
		}
	}
	int duration = 0;
	string transition;
	auto row = 2;
	for (const auto &it : transition_table) {
		if (!fromScene.isEmpty() && !QString::fromUtf8(it.first.c_str()).contains(fromScene, Qt::CaseInsensitive))
			continue;
		for (const auto &it2 : it.second) {
			if (!toScene.isEmpty() && !QString::fromUtf8(it2.first.c_str()).contains(toScene, Qt::CaseInsensitive))
				continue;
			auto col = 0;
			auto *label = new QLabel(QString::fromUtf8(it.first.c_str()));
			if (it.first == "Any") {
				label->setProperty("themeID", "good");
				label->setText(QString::fromUtf8(obs_module_text("Any")));
			} else {
				auto scene = obs_get_source_by_name(it.first.c_str());
				if (scene) {
					obs_source_release(scene);
				} else {
					label->setProperty("themeID", "error");
				}
			}
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(QString::fromUtf8(it2.first.c_str()));
			if (it2.first == "Any") {
				label->setProperty("themeID", "good");
				label->setText(QString::fromUtf8(obs_module_text("Any")));
			} else {
				auto scene = obs_get_source_by_name(it2.first.c_str());
				if (scene) {
					obs_source_release(scene);
				} else {
					label->setProperty("themeID", "error");
				}
			}
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(QString::fromUtf8(it2.second.transition.c_str()));
			mainLayout->addWidget(label, row, col++);
			label = new QLabel(QString::fromUtf8((to_string(it2.second.duration) + "ms").c_str()));
			mainLayout->addWidget(label, row, col++, Qt::AlignRight);
			auto *checkBox = new QCheckBox;
			mainLayout->addWidget(checkBox, row, col++, Qt::AlignCenter);
			duration = it2.second.duration;
			transition = it2.second.transition;
			row++;
		}
	}
	if (row == 3) {
		if (duration)
			durationSpin->setValue(duration);
		if (!transition.empty())
			transitionCombo->setCurrentText(transition.c_str());
	}
	
	selectSetCombo->setCurrentText(tt_current_setname);
	if (tt_default_setname != "") {
		int indexDefault = selectSetCombo->findText(tt_default_setname);
		selectSetCombo->setItemData(indexDefault, QBrush(Qt::black), Qt::ForegroundRole);
	}
	
	ignoreDefaultCheckBox->setChecked(tt_ignore_default_set);
	
	if (transition_table_enabled)
		set_transition_overrides();
}

void TransitionTableDialog::SelectSetChanged()
{
	QString current_set = selectSetCombo->currentText();
	tt_current_setname = current_set;
	load_set_function();
	fromCombo->setCurrentIndex(1);
	fromCombo->setCurrentIndex(0);
	toCombo->setCurrentIndex(0);
	transitionCombo->setCurrentIndex(0);
	durationSpin->setValue(500);
}

void TransitionTableDialog::NewSetClicked()
{
	bool ok;
	QString setname = QInputDialog::getText(this, obs_module_text("NewSetWindowTitle"), obs_module_text("NewSetWindowName"), QLineEdit::Normal, "", &ok);
	if (ok && !setname.isEmpty()) {
		if (selectSetCombo->findText(setname) == -1) {
			obs_data_t *obj = obs_data_create();
			tt_setlist_list_elem_t *new_elem = setlist_create_elem();
			setlist_append_elem(&setlist_ptr, new_elem, setname.toUtf8().constData());
			selectSetCombo->addItem(setname);
			selectSetCombo->setCurrentText(setname);
			tt_current_setname = setname;
			transition_table.clear();
			RefreshTable();
			save_set_function();
		} else {
			QMessageBox msgBox;
			msgBox.setWindowTitle(obs_module_text("Error"));
			msgBox.setText("Set \"" + setname + "\" " + obs_module_text("AlreadyExists"));
			msgBox.setStandardButtons(QMessageBox::Ok);
			QAbstractButton *okButton = msgBox.button(QMessageBox::Ok);
			okButton->setText(obs_module_text("Okay"));
			if (msgBox.exec() == QMessageBox::Ok) {
			}
		}
	}
}


void TransitionTableDialog::DuplicateSetClicked()
{
	QString setname = tt_current_setname + "-" + obs_module_text("Copy");
	tt_setlist_list_elem_t *new_elem = setlist_create_elem();
	setlist_append_elem(&setlist_ptr, new_elem, setname.toUtf8().constData());
	tt_current_setname = setname;
	save_set_function();
	selectSetCombo->addItem(setname);
	selectSetCombo->setCurrentText(setname);
	RefreshTable();
	
}

void TransitionTableDialog::RenameSetClicked() 
{
	bool ok;
	QString setname = QInputDialog::getText(this,
		obs_module_text("RenameSetWindowTitle1") + tt_current_setname + obs_module_text("RenameSetWindowTitle2"),
		obs_module_text("NewSetWindowName"), QLineEdit::Normal, tt_current_setname, &ok);
	if (ok && !setname.isEmpty()) {
		if (selectSetCombo->findText(setname) == -1) {
			tt_setlist_list_elem_t *old_elem = setlist_get_elem(setlist_ptr, tt_current_setname.toUtf8().constData());
			obs_data_array_t *data = obs_hotkey_save(old_elem->hotkey_id);
			tt_setlist_list_elem_t *new_elem = setlist_create_elem();
			setlist_append_elem(&setlist_ptr, new_elem, setname.toUtf8().constData());
			obs_hotkey_load(new_elem->hotkey_id, data);
			new_elem->transition_array = old_elem->transition_array;
			if (QString::fromUtf8(old_elem->setname) == tt_default_setname) {
				tt_default_setname = setname;
			}
			setlist_destroy_elem(&setlist_ptr, old_elem);
			selectSetCombo->setItemText(selectSetCombo->currentIndex(), setname);
			tt_current_setname = setname;
		} else {
			QMessageBox msgBox;
			msgBox.setWindowTitle(obs_module_text("Error"));
			msgBox.setText("Set '" + setname + "' " + obs_module_text("AlreadyExists"));
			msgBox.setStandardButtons(QMessageBox::Ok);
			QAbstractButton *okButton = msgBox.button(QMessageBox::Ok);
			okButton->setText(obs_module_text("Okay"));
			if (msgBox.exec() == QMessageBox::Ok) {
			}
		}
	}
}

void TransitionTableDialog::DeleteSetClicked()
{
	QMessageBox msgBox;
	if (setlist_ptr->next_elem == NULL) {
		msgBox.setWindowTitle(obs_module_text("Error"));
		msgBox.setText(obs_module_text("LastSetDeletionReject"));
		msgBox.setStandardButtons(QMessageBox::Ok);
		QAbstractButton *okButton = msgBox.button(QMessageBox::Ok);
		okButton->setText(obs_module_text("Okay"));
		if (msgBox.exec() == QMessageBox::Ok) {
		}
		return;
	}
	msgBox.setWindowTitle(obs_module_text("ConfirmDeletion"));
	msgBox.setText(obs_module_text("ConfirmDeletionQuestion1") + tt_current_setname + obs_module_text("ConfirmDeletionQuestion2"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	QAbstractButton *yesButton = msgBox.button(QMessageBox::Yes);
	yesButton->setText(obs_module_text("Yes"));
	QAbstractButton *noButton = msgBox.button(QMessageBox::No);
	noButton->setText(obs_module_text("No"));
	if (msgBox.exec() == QMessageBox::Yes) {
		QString current_set = "transition-tables/" + tt_current_setname;
		setlist_destroy_elem(&setlist_ptr, tt_current_setname.toUtf8().constData());
		QDir().remove(current_set);
		if (selectSetCombo->currentText() == tt_default_setname) {
			tt_default_setname = "";
		}
		int indexDelete = selectSetCombo->currentIndex();
		if (tt_ignore_default_set == true || tt_default_setname == "") {
			selectSetCombo->setCurrentIndex(0);
		} else {
			selectSetCombo->setCurrentText(tt_default_setname);
		}
		tt_current_setname = selectSetCombo->currentText();
		selectSetCombo->removeItem(indexDelete);
	}
}

void TransitionTableDialog::SetDefaultSetClicked()
{
	if (tt_default_setname != "") {
		int indexDefault = selectSetCombo->findText(tt_default_setname);
		selectSetCombo->setItemData(indexDefault, QBrush(Qt::white), Qt::ForegroundRole);
	}
	tt_default_setname = selectSetCombo->currentText();
	RefreshTable();
}	

void TransitionTableDialog::IgnoreDefaultChanged()
{
	tt_ignore_default_set = ignoreDefaultCheckBox->isChecked();
}

void TransitionTableDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget *widget = childAt(event->pos());
	if (!widget)
		return;
	int index = mainLayout->indexOf(widget);
	if (index < 0)
		return;

	int row, column, row_span, col_span;
	mainLayout->getItemPosition(index, &row, &column, &row_span, &col_span);
	if (row < 2)
		return;
	QLayoutItem *item = mainLayout->itemAtPosition(row, 0);
	if (!item)
		return;
	auto label = dynamic_cast<QLabel *>(item->widget());
	if (!label)
		return;
	const QString from = label->text();
	if (from.isEmpty())
		return;
	item = mainLayout->itemAtPosition(row, 1);
	if (!item)
		return;
	label = dynamic_cast<QLabel *>(item->widget());
	if (!label)
		return;
	const QString to = label->text();
	if (to.isEmpty())
		return;
	fromCombo->setCurrentText(from);
	toCombo->setCurrentText(to);
}


void TransitionTableDialog::ShowMatrix()
{
	// const auto md = new QDialog(this);
	md = new QDialog(this);
	md->setWindowTitle(
		QString::fromUtf8(obs_module_text("TransitionMatrix")));
	// md->setAttribute(Qt::WA_DeleteOnClose);
	md->setSizeGripEnabled(true);

	std::list<std::string> scenes;
	for (const auto &it : transition_table) {
		if (it.first != "Any")
			scenes.push_back(it.first);
		for (const auto &it2 : it.second) {
			if (it2.first != "Any")
				scenes.push_back(it2.first);
		}
	}
	scenes.sort();
	scenes.unique();
	scenes.push_front("Any");
	const int s = (int)scenes.size();
	const auto w = new QTableWidget(s, s);
	int i = 0;
	for (const auto &it : scenes) {
		w->setHorizontalHeaderItem(
			i, new QTableWidgetItem(
				   QString::fromUtf8(
					   it == "Any" ? obs_module_text("Any")
						       : it.c_str()),
				   QTableWidgetItem::ItemType::Type));
		w->setVerticalHeaderItem(
			i, new QTableWidgetItem(
				   QString::fromUtf8(
					   it == "Any" ? obs_module_text("Any")
						       : it.c_str()),
				   QTableWidgetItem::ItemType::Type));
		i++;
	}
	int row = 0;
	for (const auto &it : scenes) {
		auto f1 = transition_table.find(it);
		int column = 0;
		for (const auto &it2 : scenes) {
			string t;
			if (f1 != transition_table.end()) {
				auto f2 = f1->second.find(it2);
				if (f2 != f1->second.end()) {
					t = f2->second.transition;
				}
			}
			w->setCellWidget(
				row, column,
				new QLabel(QString::fromUtf8(t.c_str())));
			column++;
		}
		row++;
	}
	const auto m = new QVBoxLayout;
	m->addWidget(w);

	QPushButton *closeButton =
		new QPushButton(QString::fromUtf8(obs_module_text("Close")));
	// connect(closeButton, &QPushButton::clicked, [md]() { md->close(); });
	connect(closeButton, &QPushButton::clicked, [this]() { md->close(); });
	QHBoxLayout *bottomLayout = new QHBoxLayout;
	bottomLayout->addWidget(
		new QLabel(
			"<a href=\"https://obsproject.com/forum/resources/transition-table.1174/\">Transition Table</a> (" PROJECT_VERSION
			") by <a href=\"https://www.exeldro.com\">Exeldro</a>"),
		0, Qt::AlignLeft);
	bottomLayout->addWidget(closeButton, 0, Qt::AlignRight);
	m->addLayout(bottomLayout);
	md->setLayout(m);

	md->setGeometry(geometry().x(), geometry().y(), 400, 200);
	/* if (transition_table_width > min_width &&
	    transition_table_height > min_height) {
		resize(md_width, md_height);
	}*/

	// md->exec();
	md->show();
}


void TransitionTableDialog::RefreshMatrix()
{
	if (md->isVisible()) {
		/*close matrix, */
		md->close();
		ShowMatrix();
	}
}


void TransitionTableRefresh::signalRefresh()
{
	emit sigRefresh();
}
