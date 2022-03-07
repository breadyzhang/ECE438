#include<string>
#include<iostream>
#include<vector>
#include<map>
#include<fstream>

using namespace std;
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

int nextHop(pair<int,int> path, map<pair<int,int>,int> p){
  pair<int,int> curr = path;
  if(path.first == path.second){
    return path.first;
  }
  while(p[curr] != path.first){
    curr = pair<int,int>(path.first, p[curr]);
  }
  return curr.second;
}

vector<int> getPath(pair<int,int> path, map<pair<int,int>,int>p){
  vector<int> out;
  if(p.find(path) == p.end())
    return out;
  pair<int,int> curr = path;
  while(p[curr] != path.first){
    out.push_back(p[curr]);
    curr = pair<int,int>(path.first, p[curr]);
  }
  out.push_back(path.first);
  return out;
}

void bellmanFord(map<pair<int,int>,int> c, map<pair<int,int>,int>& d, map<pair<int,int>,int>& p){
  vector<int> nodes;
  map<int,int> allNodes;
  for(map<pair<int,int>,int>::iterator it = c.begin(); it != c.end(); it++){
    allNodes[it->first.first] = 1;
    allNodes[it->first.second] = 1;
  }
  for(map<int,int>::iterator it = allNodes.begin(); it != allNodes.end(); it++){
    nodes.push_back(it->first);
  }

  for(int u : nodes){
    // cout<<u<<endl;
    // init graph
    for(int v : nodes){
      pair<int,int>path(u,v);
      d[path] = -1;
      p[path] = 0;
    }
    d[pair<int,int>(u,u)] = 0;

    // relax edges repeatedly
    for(int repeat = 1; repeat < nodes.size(); repeat++){
      for(int w : nodes){
        if(u == w)
          continue;
        pair<int,int>uw(u,w);
        for(int v : nodes){
          if(v == w)
            continue;
          pair<int,int>uv(u,v);
          pair<int,int>vw(v,w);
          if(d[uv] != -1 && c.find(vw) != c.end()){
            if(d[uw] == -1){
              d[uw] = d[uv] + c[vw];
              p[uw] = v;
            }
            else if(d[uv]+c[vw] < d[uw]){
              d[uw] = d[uv] + c[vw];
              p[uw] = v;
            }
            // else if(d[uv] + c[vw] == d[uw]){
            //   int next_uw = nextHop(uw, p);
            //   int next_uv = nextHop(uv, p);
            //   if(next_uw < next_uv){
            //     p[uw] = v;
            //   }
            // }
          }
        }
      }
    }
  }
}
int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    ifstream topolfile;
    topolfile.open(argv[1]);
    ifstream messagefile;
    messagefile.open(argv[2]);
    ifstream changesfile;
    changesfile.open(argv[3]);
    ofstream output("output.txt");

    map<pair<int,int>,int> linkCost; // mapping of direct link cost
    map<pair<int,int>,int> pathCost; // mapping of cost from x to y with dijkstra's
    map<pair<int,int>,int> predecessor;
    int maxNode = 0;
    string line;
    // getting topology
    while(getline(topolfile, line)){
      vector<string> nodes;
      int index = 0;
      int next = line.find(" ");
      while(index != string::npos){
        nodes.push_back(line.substr(index, next));
        index = next;
        next = line.find(" ", index+1);
      }
      int x = stringToInt(nodes[0]);
      int y = stringToInt(nodes[1]);
      int c = stringToInt(nodes[2]);
      maxNode = max(maxNode, x);
      maxNode = max(maxNode, y);
      pair<int,int> path1(x,y);
      pair<int,int> path2(y,x);
      if(c < 0){
        map<pair<int,int>,int>::iterator it1 = linkCost.find(path1);
        map<pair<int,int>,int>::iterator it2 = linkCost.find(path2);
        if(it1 != linkCost.end()){
          linkCost.erase(it1);
          linkCost.erase(it2);
        }
      }
      else{
        linkCost[path1] = c;
        linkCost[path2] = c;
      }
    }
    // getting message source and dests
    vector<pair<int,int>>messagePath;
    vector<string>messageContent;
    while(getline(messagefile, line)){
      vector<string> nodes;
      int delim = line.find(" ");
      int dest = line.find(" ", delim+1);
      int src = stringToInt(line.substr(0,delim));
      string msg = line.substr(dest+1);
      dest = stringToInt(line.substr(delim,dest));
      messagePath.push_back(pair<int,int>(src,dest));
      messageContent.push_back(msg);
    }
    bellmanFord(linkCost, pathCost, predecessor);
    // cout<<"done with bellman"<<endl;
    for(int i = 1; i <= maxNode; i++){
      // cout<<"shortest paths for "<<i<<endl;
      for(int j = 1; j <= maxNode; j++){
        pair<int,int>path(i,j);
        // if(predecessor.find(path) != predecessor.end()){
        //   cout<<i<<" "<<j<<" "<<nextHop(path,predecessor)<<endl;
        // }
        if(pathCost.find(path) != pathCost.end() && pathCost[path] >= 0)
          output<<j<<" "<<nextHop(path, predecessor)<<" "<<pathCost[path]<<endl;
      }
    }
    // find message path
    for(int i = 0; i < messagePath.size(); i++){
      // “from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>”
      vector<int> walk;
      int u = messagePath[i].first;
      int v = messagePath[i].second;
      pair<int,int>path(u, v);
      walk = getPath(path, predecessor);
      if(walk.size() == 0){
        //“from <x> to <y> cost infinite hops unreachable message <message>”
        output<<"from "<<u<<" to "<<v<<" cost infinite hops unreachable message "<<messageContent[i]<<endl;
      }
      else{
        output<<"from "<<u<<" to "<<v<<" cost "<< pathCost[path]<<" hops ";
        for(int i = walk.size()-1; i >= 0; i--)
          output<<walk[i]<<" ";
        output<<"message "<<messageContent[i]<<endl;
      }
    }
    // update topology
    while(getline(changesfile, line)){
      pathCost.clear();
      predecessor.clear();
      vector<string> nodes;
      int index = 0;
      int next = line.find(" ");
      while(index != string::npos){
        nodes.push_back(line.substr(index, next));
        index = next;
        next = line.find(" ", index+1);
      }
      int x = stringToInt(nodes[0]);
      int y = stringToInt(nodes[1]);
      int c = stringToInt(nodes[2]);
      maxNode = max(maxNode, x);
      maxNode = max(maxNode, y);
      pair<int,int> path1(x,y);
      pair<int,int> path2(y,x);
      if(c < 0){
        map<pair<int,int>,int>::iterator it1 = linkCost.find(path1);
        map<pair<int,int>,int>::iterator it2 = linkCost.find(path2);
        if(it1 != linkCost.end()){
          linkCost.erase(it1);
          linkCost.erase(it2);
        }
      }
      else{
        linkCost[path1] = c;
        linkCost[path2] = c;
      }
      // for(map<pair<int,int>,int>::iterator it = linkCost.begin(); it != linkCost.end(); it++){
      //   cout<<it->first.first<<" "<<it->first.second<<" "<<it->second<<endl;
      // }
      bellmanFord(linkCost, pathCost, predecessor);
      for(int i = 1; i <= maxNode; i++){
        // cout<<"shortest paths for "<<i<<endl;
        for(int j = 1; j <= maxNode; j++){
          pair<int,int>path(i,j);
          if(pathCost.find(path) != pathCost.end() && pathCost[path] >= 0)
            output<<j<<" "<<nextHop(path, predecessor)<<" "<<pathCost[path]<<endl;
        }
      }
      // write message and find path
      for(int i = 0; i < messagePath.size(); i++){
        // “from <x> to <y> cost <path_cost> hops <hop1> <hop2> <...> message <message>”
        vector<int> walk;
        int u = messagePath[i].first;
        int v = messagePath[i].second;
        pair<int,int>path(u, v);
        walk = getPath(path, predecessor);
        if(walk.size() == 0){
          //“from <x> to <y> cost infinite hops unreachable message <message>”
          output<<"from "<<u<<" to "<<v<<" cost infinite hops unreachable message "<<messageContent[i]<<endl;
        }
        else{
          output<<"from "<<u<<" to "<<v<<" cost "<< pathCost[path]<<" hops ";
          for(int i = walk.size()-1; i >= 0; i--)
            output<<walk[i]<<" ";
          output<<"message "<<messageContent[i]<<endl;
        }
      }
    }
    output.close();
    return 0;
}
