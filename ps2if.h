
#ifndef PS2IF_H
#define PS2IF_H

void pc_set_clock_0();
void pc_set_clock_1();
void pc_set_data_0();
void pc_set_data_1();
bool pc_get_clock();
bool pc_get_data();
void kb_set_clock_0();
void kb_set_clock_1();
void kb_set_data_0();
void kb_set_data_1();
bool kb_get_clock();
bool kb_get_data();

class AbstractPS2IO {
public:
	virtual void set_clock_0() = 0;
	virtual void set_clock_1() = 0;
	virtual void set_data_0() = 0;
	virtual void set_data_1() = 0;
	virtual bool get_clock() = 0;
	virtual bool get_data() = 0;
};


class PS2KB1 : public AbstractPS2IO {
public:
	void set_clock_0() override
	{
		kb_set_clock_0();
	}
	void set_clock_1() override
	{
		kb_set_clock_1();
	}
	void set_data_0() override
	{
		kb_set_data_0();
	}
	void set_data_1() override
	{
		kb_set_data_1();
	}
	bool get_clock() override
	{
		return kb_get_clock();
	}
	bool get_data() override
	{
		return kb_get_data();
	}
};

#endif
