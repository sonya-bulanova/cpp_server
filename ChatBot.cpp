#include <iostream>
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

const string PRIVATE_MSG = "PRIVATE_MSG";
const string PUBLIC_MSG = "PUBLIC_MSG";
const string STATUS = "STATUS";
const string SET_NAME = "SET_NAME";
const string PUBLIC_CHANNEL = "PUBLIC_CHANNEL";

struct UserData {
	int id;
	string name;
};

string user_status(UserData* data, bool online) {
	json payload = {
		{"command", STATUS},
		{"online", online},
		{"user_id", data->id},
		{"name", data->name}
	};
	return payload.dump();
}
//Записывать тип покроче 
typedef uWS::WebSocket<false, true, UserData>* websock;

void process_private_messages(websock ws, json parsed_data) {
	json payload;
	UserData* data = ws->getUserData();
	payload["command"] = PRIVATE_MSG;
	payload["user_from"] = data->id;
	payload["text"] = parsed_data["text"];

	ws->publish("USER_" + to_string(parsed_data["user_to"]), payload.dump());
}

void process_public_message(websock ws, json parsed_data) {
	UserData* data = ws->getUserData();

	json payload = {
		{"command", PUBLIC_MSG},
		{"text", parsed_data["text"]},
		{"user_from", data->id}
	};
	ws->publish(PUBLIC_CHANNEL, payload.dump());
}

void process_set_name(websock ws, json parsed_data) {
	UserData* data = ws->getUserData();
	data->name = parsed_data["name"];//обновили имя
	ws->publish(PUBLIC_CHANNEL, user_status(data, true));
}

map<int, UserData*> all_users;

int main() {
	int latest_user_id = 10;

	uWS::App app = uWS::App();

	//Создали раздел "/" на сайте
	app.get("/", [](auto *response, auto *request) {
		response->writeHeader("Content-Type", "text/html; charset=ut8");
		response->end("Hello, this is StarShip C++ Server");
	});
	app.ws<UserData>("/*", {
		//Лямбда-функиця (?)
		//открытие соединения
		.open = [&latest_user_id] (websock ws) {
			UserData* data = ws->getUserData();
			data->id = latest_user_id++;
			all_users[data->id] = data; //положить инфу о пользователе в карту

			cout << "New user connected " << data->id << endl;
			data->name = "Unnamed" + to_string(data->id);
			
			ws->subscribe("USER_" + to_string(data->id));

			ws->publish(PUBLIC_CHANNEL, user_status(data, true));

			ws->subscribe(PUBLIC_CHANNEL);
			
			for (auto item : all_users) {
				ws->send(user_status(item.second, true), uWS::OpCode::TEXT);
			}
		},

		.message = [](websock ws, string_view message, uWS::OpCode opcode) {
			json parsed_data;

			try{
				parsed_data = json::parse(message);
			}
			catch (const json::parse_error& err) {
				cout << err.what() << endl;
				return;
			}

			string command = parsed_data["command"];

			cout << "Got command: " << command << endl;
			if (command == PRIVATE_MSG) {
				process_private_messages(ws, parsed_data);
			} 

			if (command == PUBLIC_MSG) {
				process_public_message(ws, parsed_data);
			}

			if (command == SET_NAME) {
				process_set_name(ws, parsed_data);
			}
		},

		.close = [&app](websock ws, int, string_view) {
			UserData* data = ws->getUserData();
			app.publish(PUBLIC_CHANNEL, user_status(data, false), uWS::OpCode::TEXT);
			all_users.erase(data->id);
		},
		});

	app.listen(9001, [](auto *) {});
	app.run();
}
