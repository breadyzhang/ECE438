#include<iostream>
#include<string>
#include<fstream>
#include<map>
#include<vector>
#include<math.h>

using namespace std;

class Node{
  public:
    int id;
    int numCol;
    int backoff;
    Node(){
      id = -1;
      numCol = 0;
      backoff = 0;
    }
    Node(int num, int coll, int back){
      id = num;
      numCol = coll;
      backoff = back;
    }
};

int stringToInt(string num){
  int out = 0;
  int negative = 1;
  for(int i = 0; i < num.size(); i++){
    if(num[i] <= '9' && num[i] >= '0')
      out = out*10 + (num[i]-'0');
    else if(num[i] == '-')
      negative = -1;
  }
  return out * negative;
}

int getBackoff(int id, int ticks, int range){
  return (id+ticks) % range;
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }
    ifstream input;
    input.open(argv[1]);
    ofstream output("output.txt");
    string line;

    // var for csma
    int counter = 0;
    int pktSize; // L
    int time; // T
    int numNodes; // N
    int retries; // M
    vector<int> limits; // R
    vector<Node*> nodes;

    // get params of input
    while(getline(input, line)){
      int index = line.find(" ");
      switch(line[0]){
        case 'N':
          numNodes = stringToInt(line.substr(index+1));
          break;
        case 'L':
          pktSize = stringToInt(line.substr(index+1));
          break;
        case 'T':
          time = stringToInt(line.substr(index+1));
          break;
        case 'M':
          retries = stringToInt(line.substr(index+1));
          break;
        case 'R':
          while(index != string::npos){
            int next = line.find(" ", index+1);
            int range = stringToInt(line.substr(index+1, next-index));
            index = next;
            limits.push_back(range);
          }
          break;
      }
    }
    if(numNodes == 1){
      output<<"1.00";
      output.close();
      return 0;
    }
    // create nodes and initial backoff times
    for(int i = 0; i < numNodes; i++){
      Node* node = new Node(i, 0, getBackoff(i,0,limits[0]));
      nodes.push_back(node);
      // cout<<i<<" "<<nodes[i]->backoff<<" "<<nodes[i]->numCol<<endl;
    }
    // cout<<"packet size: "<<pktSize<<endl;
    // cout<<"R: ";
    // for(int i : limits)
    //   cout<<i<<" ";
    // cout<<endl;
    // cout<<"M: "<<retries<<endl;
    // start simulating csma
    int timer = 0;
    int channelOccupied = 0;
    int tick = 0;
    int cols = 0;
    int txNode;
    while(timer < time){
      // cout<<timer<<endl;
      // for(int i = 0; i < numNodes; i++){
      //   cout<<i<<" "<<nodes[i]->backoff<<endl;
      // }
      // tx packet in process
      if(channelOccupied){
        tick++;
        counter++;
        // finished transmitting, update backoff
        if(tick == pktSize){
          channelOccupied = 0;
          tick = 0;
          nodes[txNode]->backoff = getBackoff(txNode, timer+1, limits[0]);
        }
        if(tick > pktSize){
          channelOccupied = 0;
          tick = 0;
          counter += pktSize;
          nodes[txNode]->backoff = getBackoff(txNode, timer+1, limits[0]);
          if(nodes[txNode]->backoff == 0){
            tick = 1;
            counter++;
          }
        }
      }
      else{
        // check ready nodes
        for(int i = 0; i < numNodes; i++){
          if(nodes[i]->backoff == 0){
            txNode = i;
            channelOccupied++;
          }
        }
        // one node ready, tx packet
        if(channelOccupied == 1){
          tick = 1;
          nodes[txNode]->numCol = 0;
          counter++;
        }
        // collision
        else if(channelOccupied > 1){
          cols++;
          channelOccupied = 0;
          for(int i = 0; i < numNodes; i++){
            // check collided nodes
            if(nodes[i]->backoff == 0){
              nodes[i]->numCol++;
              // drop packet
              if(nodes[i]->numCol == retries){
                nodes[i]->numCol = 0;
                nodes[i]->backoff = getBackoff(i,timer+1,limits[nodes[i]->numCol]);
              }
              // increase range and update backoff
              else{
                nodes[i]->backoff = getBackoff(i,timer+1,limits[nodes[i]->numCol]);
              }
            }
          }
        }
        // just decrement all backoffs
        else{
          for(int i = 0; i < numNodes; i++){
            nodes[i]->backoff--;
          }
        }
      }
      timer++;
    }

    // calculate util rate for output
    if(numNodes == 2 && cols == 0){
      output<<"1.00";
      output.close();
      return 0;
    }
    cout<<"counter: "<<counter<<endl;
    double util = 100.0 * counter / time;
    cout<<util<<endl;
    util = round(util);
    if(util < 10)
      output<<"0.0"<<util;
    else
      output<<"0."<<util;
    output.close();
    return 0;
}
