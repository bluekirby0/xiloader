#include <mutex>
#include <string>

typedef unsigned long DWORD;

namespace xiloader
{
    class globals
    {
    private:
        std::mutex _global_mutex;

        std::string _serverAddress;		// The server address to connect to.
        bool _hide; 				// Determines whether or not to hide the console window after FFXI starts.
        //DWORD _newServerAddress; 		// Disabled until a better way of handling the code cave is found
        //DWORD _hairpinReturnAddress;
        float _drawDistance[1];			// Distance at which terrain should be drawn
        float _mobDistance[1];			// Distance at which mobs should be drawn. >30 will likely have no effect in most cases.

    public:
        globals() : _serverAddress("127.0.0.1"),
        _hide(false),
        //_newServerAddress(0),
        //_hairpinReturnAddress(0),
        _drawDistance {20.0f},
        _mobDistance {20.0f}
        {}

        ~globals()
        {
            //delete _drawDistance;
            //delete _mobDistance;
        }

        void setServerAddress(std::string addr);
        void getServerAddress(std::string* addr);
        void setHide(bool state);
        void getHide(bool* state);
        //void setNewServerAddress(DWORD addr);
        //void getNewServerAddress(DWORD *addr);
        //void setHairpinReturnAddress(DWORD addr);
        //void getHairpinReturnAddress(DWORD *addr);
        void setDrawDistance(float dist);
        void getDrawDistance(float* dist);
        float* getDrawDistanceUnsafe();
        void setMobDistance(float dist);
        void getMobDistance(float* dist);
        float* getMobDistanceUnsafe();
    };
}
