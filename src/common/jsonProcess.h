#ifndef JSON_PROCESS_H
#define JSON_PROCESS_H

#include <bits/stdc++.h>
#include "../common/common.h"
using namespace std;

void read_json(string testcase_config_path, vector<Task> &taskData);

void init_dependencies(vector<Task> &taskData, vector<vector<int>> &dependency_counts, vector<vector<vector<int>>> &dependency_edges);

#endif