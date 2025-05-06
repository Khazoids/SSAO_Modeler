#pragma once
class ClientState
{
public:
	float x;
	float y;
	float z;
	bool rightMouseDown;
	bool leftMouseDown;
	bool spaceKeyToggled;

	ClientState() : x(0), y(0), z(0), rightMouseDown(false), leftMouseDown(false), spaceKeyToggled(false) {}
	~ClientState() {};
};