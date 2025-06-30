int motor = 16; // Motor habilitado
#define dt 4
#define p1 1
#define p2 4
#define p3 2
#define p4 8
#define EM 16

// Variáveis de posição (coordenadas atuais)
float currentX = 0;
float currentY = 0;
float currentZ = 0;
int Mx, My, Mz;
float stepsPerCm = 1000.0 / 8; // Passos por centímetro

bool shouldStop = false;
bool penDown = false;
bool emergencyActive = false;

// Declarações antecipadas das funções
void moveAxis(int steps, void (*setPort)(int), bool checkStop = true);
void moveX(int steps);
void moveY(int steps);
void moveZ(int steps, bool checkStop = true);
void checkForStopCommand();
void emergencyStop();
void drawSquare(float sideLength);
void resetCoordinates();
void homing();
void updatePosition(char axis, float distance);

void setup() {
  // Configuração dos pinos
  pinMode(14, OUTPUT);       // A0 - PC0
  pinMode(15, OUTPUT);       // A1 - PC1
  pinMode(16, OUTPUT);       // A2 - PC2
  pinMode(17, OUTPUT);       // A3 - PC3
  pinMode(18, OUTPUT);       // A4 - PC4

  Mx = EM | p1;
  PORTC = Mx;

  pinMode(2, OUTPUT);       // PD2
  pinMode(3, OUTPUT);       // PD3
  pinMode(4, OUTPUT);       // PD4
  pinMode(5, OUTPUT);       // PD5
  pinMode(6, OUTPUT);       // PD6

  My = EM | p1;
  PORTD = My << 2;
  
  pinMode(8, OUTPUT);       // PB0
  pinMode(9, OUTPUT);       // PB1
  pinMode(10, OUTPUT);      // PB2
  pinMode(11, OUTPUT);      // PB3
  pinMode(12, OUTPUT);      // PB4

  Mz = EM | p1;
  PORTB = Mz;
  
  Serial.begin(9600);
  Serial.println("Iniciando...");
  Serial.println("Opcoes disponiveis:");
  Serial.println("1. Comando manual: [eixo] [distancia em cm]");
  Serial.println("   Exemplo: x 5.0 (move eixo x em 5 cm)");
  Serial.println("   Eixos disponiveis: x, y, z");
  Serial.println("2. Desenhar quadrado: q [tamanho do lado em cm]");
  Serial.println("   Exemplo: q 10.0 (desenha quadrado com 10 cm de lado)");
  Serial.println("3. Resetar coordenadas: r");
  Serial.println("4. Homing (mover para origem): h");
  Serial.println("Pressione 'P' a qualquer momento para parada de emergencia.");
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    if (command == 'p' || command == 'P') {
      emergencyStop();
      while(Serial.available()) Serial.read(); // Limpa buffer
      return;
    }
    
    if (command == 'q' || command == 'Q') {
      // Modo desenhar quadrado
      Serial.read(); // Lê o espaço
      float sideLength = Serial.parseFloat();
      while(Serial.available()) Serial.read(); // Limpa buffer
      
      if(sideLength > 0) {
        drawSquare(sideLength);
      } else {
        Serial.println("Valor invalido! Use valores positivos.");
      }
    } 
    else if(command == 'x' || command == 'X' || 
            command == 'y' || command == 'Y' || 
            command == 'z' || command == 'Z') {
      // Comando manual
      char axis = tolower(command);
      Serial.read(); // Lê espaço
      float distance = Serial.parseFloat();
      while(Serial.available()) Serial.read();
      
      int steps = round(distance * stepsPerCm);
      
      Serial.print("Movendo eixo ");
      Serial.print(axis);
      Serial.print(" em ");
      Serial.print(distance);
      Serial.println(" cm");
      
      shouldStop = false;
      
      switch(axis) {
        case 'x': moveX(steps); updatePosition('x', distance); break;
        case 'y': moveY(steps); updatePosition('y', distance); break;
        case 'z': moveZ(steps); updatePosition('z', distance); break;
      }
      
      Serial.print("Posicao atual: X=");
      Serial.print(currentX);
      Serial.print("cm, Y=");
      Serial.print(currentY);
      Serial.print("cm, Z=");
      Serial.print(currentZ);
      Serial.println("cm");
    }
    else if(command == 'r' || command == 'R') {
      // Comando para resetar coordenadas
      resetCoordinates();
      while(Serial.available()) Serial.read();
    }
    else if(command == 'h' || command == 'H') {
      // Comando para homing
      homing();
      while(Serial.available()) Serial.read();
    }
    else {
      Serial.println("Comando invalido!");
      while(Serial.available()) Serial.read();
    }
  }
}

void moveAxis(int steps, void (*setPort)(int), bool checkStop) {
  int direction = steps > 0 ? 1 : -1;
  steps = abs(steps);
  
  const int patterns[4] = {17, 20, 18, 24}; // Padrões de acionamento
  
  for(int i = 0; i < steps && (!checkStop || !shouldStop); i++) {
    if(!emergencyActive) {
      checkForStopCommand();
    }
    
    if(direction > 0) {
      for(int j = 0; j < 4; j++) {
        setPort(patterns[j]);
        delay(dt);
      }
    } else {
      for(int j = 3; j >= 0; j--) {
        setPort(patterns[j]);
        delay(dt);
      }
    }
  }
}

void moveX(int steps) {
  moveAxis(steps, [] (int pattern) { PORTC = pattern; });
}

void moveY(int steps) {
  moveAxis(steps, [] (int pattern) { PORTD = pattern << 2; });
}

void moveZ(int steps, bool checkStop) {
  moveAxis(steps, [] (int pattern) { PORTB = pattern; }, checkStop);
}

void checkForStopCommand() {
  if(Serial.available() > 0) {
    char c = Serial.read();
    if(c == 'p' || c == 'P') {
      emergencyStop();
    }
    while(Serial.available()) Serial.read();
  }
}

void emergencyStop() {
  if(emergencyActive) return;
  
  emergencyActive = true;
  shouldStop = true;
  Serial.println("\n--- PARADA DE EMERGENCIA ---");
  
  if(penDown) {
    Serial.println("Levantando caneta...");
    moveZ(-round(8.5 * stepsPerCm), false); // Move sem verificar stop
    penDown = false;
    currentZ = 0; // Considera que levantou completamente
    Serial.println("Caneta retornou para posicao segura");
  }
  
  Serial.println("Sistema pronto para novos comandos");
  emergencyActive = false;
}

void drawSquare(float sideLength) {
  int distancePenToBoard = round(8.5 * stepsPerCm);
  int steps = round(sideLength * stepsPerCm);
  
  shouldStop = false;
  emergencyActive = false;
  
  Serial.println("\nIniciando desenho do quadrado...");
  
  // Abaixa caneta
  Serial.println("Abaixando caneta...");
  moveZ(distancePenToBoard);
  penDown = true;
  currentZ = 8.5; // Define posição Z como abaixada
  if(shouldStop) {
    emergencyStop();
    return;
  }

  // Array com movimentos para cada lado
  struct {
    const char* name;
    void (*move)(int);
    int steps;
    char axis;
    float distance;
  } sides[4] = {
    {"Lado 1: X+", moveX, steps, 'x', sideLength},
    {"Lado 2: Y+", moveY, steps, 'y', sideLength},
    {"Lado 3: X-", moveX, -steps, 'x', -sideLength},
    {"Lado 4: Y-", moveY, -steps, 'y', -sideLength}
  };

  for(int i = 0; i < 4 && !shouldStop; i++) {
    Serial.println(sides[i].name);
    sides[i].move(sides[i].steps);
    updatePosition(sides[i].axis, sides[i].distance);
    
    if(shouldStop) {
      emergencyStop();
      return;
    }
  }
  
  // Levanta caneta
  Serial.println("Levantando caneta...");
  moveZ(-distancePenToBoard);
  penDown = false;
  currentZ = 0;
  
  Serial.println("Quadrado completo!\n");
}

void resetCoordinates() {
  currentX = 0;
  currentY = 0;
  currentZ = 0;
  Serial.println("Coordenadas resetadas para (0, 0, 0)");
  Serial.print("Posicao atual: X=");
  Serial.print(currentX);
  Serial.print("cm, Y=");
  Serial.print(currentY);
  Serial.print("cm, Z=");
  Serial.print(currentZ);
  Serial.println("cm");
}

void homing() {
  Serial.println("Iniciando sequencia de homing...");
  shouldStop = false;
  emergencyActive = false;
  
  // 1. Levantar a caneta primeiro (se estiver abaixada)
  if(penDown) {
    Serial.println("Levantando caneta antes do homing...");
    moveZ(-round(currentZ * stepsPerCm), false);
    penDown = false;
    currentZ = 0;
  }
  
  // 2. Mover eixo X para origem (0)
  if(currentX != 0) {
    Serial.print("Movendo eixo X para origem (atual: ");
    Serial.print(currentX);
    Serial.println("cm)");
    moveX(-round(currentX * stepsPerCm));
    currentX = 0;
  }
  
  // 3. Mover eixo Y para origem (0)
  if(currentY != 0) {
    Serial.print("Movendo eixo Y para origem (atual: ");
    Serial.print(currentY);
    Serial.println("cm)");
    moveY(-round(currentY * stepsPerCm));
    currentY = 0;
  }
  
  // 4. Mover eixo Z para origem (0) - redundante, já feito no passo 1
  if(currentZ != 0) {
    Serial.print("Movendo eixo Z para origem (atual: ");
    Serial.print(currentZ);
    Serial.println("cm)");
    moveZ(-round(currentZ * stepsPerCm), false);
    currentZ = 0;
  }
  
  Serial.println("Homing completo! Todos os eixos na posição de origem.");
  Serial.print("Posicao atual: X=");
  Serial.print(currentX);
  Serial.print("cm, Y=");
  Serial.print(currentY);
  Serial.print("cm, Z=");
  Serial.print(currentZ);
  Serial.println("cm");
}

void updatePosition(char axis, float distance) {
  switch(axis) {
    case 'x': currentX += distance; break;
    case 'y': currentY += distance; break;
    case 'z': currentZ += distance; break;
  }
}