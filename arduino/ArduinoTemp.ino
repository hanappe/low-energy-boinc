template <typename T, unsigned int Size>
struct Array {
public:

  static const int MaxSize = Size;
  
  Array() {
    memset(m_message, 0, MaxSize);
    m_size = 0;
  }

   bool operator == (const Array& p) const {
     bool res = false; 
     if (this == &p)
       return true;
      
     if (this->size() != p.size())
       return false;
       
     size_t s = this->size();
     while(s--) {
       if (this->m_message[s] != p.m_message[s]) {
         return false;
       }
     } // while

     return true;
  }
  
  // Append T data
  void push_back(T c) {
    *this << c;
  }
  
  Array& operator << (T c) {
    if (m_size < MaxSize ) {
      m_message[m_size++] =  c;
    }
    return *this;
  }

  
  void clear() {
    memset(m_message, 0, MaxSize);
    m_size = 0;
  }
  
  const T * data() {
    return (const T *)&m_message[0];
  }
  
   const char * charPtr() const {
    return (const char *)&m_message[0];
  }
  
  size_t size() const {
    return m_size;
  }
  uint8_t m_message[MaxSize];
  T m_size;
  
};

template <unsigned int Size>
struct APacket : public Array<uint8_t, Size> {

public:
  APacket() : Array<uint8_t, Size>() {
  }
  
  // Append float
  APacket& appendFloat(float f) {
    this->append((uint8_t*)(&f), 4);
    return *this;
  }
  
  // Append data
  APacket& append(uint8_t* data, size_t byteSize)
  {
      if (data && (byteSize > 0))
      {
        for (int i = 0; i < byteSize ; ++i) {
           *this << data[i];
        }
      }
      return *this;
  }
};

struct ParsingMachine : Array<char, 256> {

	// Assuming Arduino answers is like {{data}}
	// With 'data' equals anything (256 byts max)
	enum State {
		BeginToken1, //Find the first {
		BeginToken2, //Find the second {
		EndToken1, //Find the first }
		EndToken2, //Find the second }
		ValueFound // Arduino data/value found
	};

	ParsingMachine() :
		s(BeginToken1)
	{}

	void processChar(const char c) {
		switch (s) {
		case BeginToken1:
			if (c == '{') {
				s = BeginToken2;
			}
			break;
		case BeginToken2:
			if (c == '{') {
				s = EndToken1;
			} else {
				init();
			}
			break;
		case EndToken1:
			if (c == '}') {
				s = EndToken2;
			} else {
				if (this->size() < MaxSize) {
					this->push_back(c);
				} else {
					init();
				}
			}
			break;
		case EndToken2:
			if (c == '}') {
				s = ValueFound;
			} else {
				init();
			}
			break;
		default:
			break;
		}
	}

	void init() {
		this->clear();
		s = BeginToken1;
	}

	bool hasValue() const {
		return s == ValueFound;
	}

	State s;
};

ParsingMachine machine;


void setup() {
  // initialize serial:
  Serial.begin(9600);
  machine.init();
}

char id[] = {'?'};

char temp[] = {'T'};

inline size_t Min(size_t a, size_t b) {
  return a < b ? a : b;
}
void loop() {
  
  if (machine.hasValue()) {

    // If machine value == question value
    if (memcmp(machine.charPtr(), id, Min(machine.size(), sizeof(id))) == 0) { 
      
      APacket<8> p;
      p << '{' << '{';
      p << 'A';
      p << '}' << '}';
      
      Serial.write(p.data(), p.size());
      // If machine value == temperature value
    } else if (memcmp(machine.charPtr(), temp, Min(machine.size(), sizeof(temp))) == 0) {
      
      int sensorValue = analogRead(A0);
      float temperature = (sensorValue*(5000.f/1023.f) - 500) / 10.f; // Temp in celcius
        
      APacket<16> p;
      p << '{' << '{';
      p.appendFloat(temperature);
      p << '}' << '}';
      
       Serial.write(p.data(), p.size());
    }

    machine.init();
  }
 
  delay(100);
}

void serialEvent() {
  while (Serial.available()) {
    const char inChar = (char)Serial.read();
    //Serial.println(inChar);
    machine.processChar(inChar);
  } // while serial available
}



