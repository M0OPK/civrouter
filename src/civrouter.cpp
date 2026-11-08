#include <civutils.h>
#include <chrono>
#include <iostream>
#include <ostream>
#include <async_comm/serial.h>
#include <civconfig.h>
#include <civserial.h>
#include <civport.h>
#include <string>
#include <thread>
#include <vector>

int main()
{
	using namespace civ;
	using namespace std;
	auto config = civ::CivConfig("/home/peter/.config/civrouterpp.conf");

	// Open all the radio ports
	vector<CivPort*> controllerPorts;
	vector<CivPort*> radioPorts;

	// Load ports
	for(auto portName : config.GetPortNames())
	{
		auto port = new CivPort(config.GetPort(portName));
		if (port->IsRadioPort())
		{
			radioPorts.push_back(port);
			cout << "Radio port : " << portName << endl;
		}
		else
		{
			controllerPorts.push_back(port);
			cout << "Controller port : " << portName << endl;
		}
	}

	// Validate we have at least one of each kind
	if (radioPorts.empty())
	{
		cerr << "There must be at least one radio port defined!" << endl;
		return 10;
	}

	if (controllerPorts.empty())
	{
		cerr << "There must be at least one controller port defined!" << endl;
		return 10;
	}

	while (1 == 1)
	{
		bool foundData = true;
		while(foundData)
		{
			foundData = false;
			for(auto radioPort : radioPorts)
			{
				//cout << "Processing radio port " << radioPort->PortName() << endl;
				if (radioPort->Update())
				{
					while(radioPort->FrameReady())
					{
						auto frame = radioPort->PopFrame();
						//frame.DumpFrame();
						if (!frame.ValidFrame())
						{
							continue;
						}
						for(auto controllerPort : controllerPorts)
						{
							auto controllerFrame = frame;
							
							// Check if we should send a broadcast frame
							if (controllerFrame.IsBroadcast() && (!radioPort->PortSendsTranceive() || !controllerPort->PortAcceptsTranceive()))
							{
								continue;
							}
							// Check if this frame is for this destination
							if (!controllerFrame.IsBroadcast() && controllerFrame.GetToAddress() != controllerPort->VirtualAddress())
							{
								continue;
							}
							// Rewrite to address to original port address
							if (!controllerFrame.IsBroadcast())
							{
								controllerFrame.SetToAddress(controllerPort->RealAddress());
							}

							controllerPort->SendFrame(controllerFrame);
							//controllerFrame.DumpFrame();
						}
					}
					foundData = true;
				}
			}
			for(auto controllerPort : controllerPorts)
			{
				//cout << "Processing controller port " << controllerPort->PortName() << endl;
				if (controllerPort->Update())
				{
					while(controllerPort->FrameReady())
					{
						auto frame = controllerPort->PopFrame();
						if (!frame.ValidFrame())
						{
							continue;
						}
						for (auto radioPort : radioPorts)
						{
							auto radioFrame = frame;
							// Check if we should send a broadcast frame
							if (frame.IsBroadcast() && (!controllerPort->PortSendsTranceive() || !radioPort->PortAcceptsTranceive()))
							{
								continue;
							}

							// Check if this frame is for this destination
							if (!radioFrame.IsBroadcast() && radioFrame.GetToAddress() != radioPort->RealAddress())
							{
								cout << "Original: " << CivUtils::Uint8ToHex(radioFrame.GetToAddress()) << " Radio ID: " << CivUtils::Uint8ToHex(radioPort->RealAddress()) << endl;
								continue;
							}
							// Rewrite from address to controller virual port address
							if (!radioFrame.IsBroadcast())
							{
								radioFrame.SetFromAddress(controllerPort->VirtualAddress());
							}

							if (radioFrame.IsChangeCommand() && (controllerPort->PortSendsInfo()))
							{
								// Make a broadcast version of this frame and send to all other controllers
								auto changeFrame = radioFrame;
								changeFrame.MakeTransceive();
								changeFrame.SetFromAddress(radioPort->RealAddress());
								for(auto otherControllerPort : controllerPorts)
								{
									if (otherControllerPort->PortName() != controllerPort->PortName() && otherControllerPort->PortAcceptsInfo())
									{
										otherControllerPort->SendFrame(changeFrame);
									}
								}
							}
							//radioFrame.DumpFrame();

							radioPort->SendFrame(radioFrame);
						}
					}
					foundData = true;
				}
			}
		
		}
		// @ToDo: Make the wait time settable in config
		this_thread::sleep_for(milliseconds(1));
	}
	
	return 0;
}
