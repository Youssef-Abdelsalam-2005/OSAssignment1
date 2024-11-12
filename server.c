#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    bool isInteractive;
    int port;
} CmdArg;

CmdArg ParseCmdLine(int argc, char ** argv, CmdArg* cmd) {
    cmd->isInteractive = false;
    cmd->port = -1;

    if (argc == 2) {
        if (strcmp(argv[1], "-i") == 0) {
            cmd->isInteractive = true;
        } else {
            cmd->port = atoi(argv[1]);
            if (cmd->port <= 0 || cmd->port > 65535) {
                exit(1);
            }
        }
    } else {
        exit(1);
    }

    return *cmd;
}


typedef struct {
    unsigned char octet[4];
} IPAddress;

typedef struct {
    IPAddress start;
    IPAddress end;
    int isRange;
} IPRange;

typedef struct {
    unsigned short start;
    unsigned short end;
    int isRange;
} PortRange;

typedef struct query {
    IPAddress ipAddress;
    unsigned short port;
    struct rule* matchedRule;
    struct query* next;
} Query;

typedef struct rule{
    IPRange ipRange;
    PortRange portRange;
    int isAllow;
    struct rule * next;
    struct query* queries;
} Rule;


typedef struct request{
    char command[256];
    struct request * next;
} Request;

typedef struct {
    int socket;
    Request* requests;
    Rule* rules;
    Query* queries;
} ThreadArgs;

bool isValidIPNumber(int num) {
    return (num >= 0 && num <= 255);
}


bool isValidIPAddress(const char* ip_str) {
    int dotCount = 0;
    for (const char* p = ip_str; *p; p++) {
        if (*p == '.') {
            dotCount++;
        }
    }

    if (dotCount != 3) {
        return false;
    }

    int num1, num2, num3, num4;
    if (sscanf(ip_str, "%d.%d.%d.%d", &num1, &num2, &num3, &num4) != 4) {
        return false;
    }

    return isValidIPNumber(num1) && isValidIPNumber(num2) && 
           isValidIPNumber(num3) && isValidIPNumber(num4);
}

IPAddress parseIPAddress(const char* ip_str) {
    IPAddress ip = {{0}}; 
    if (!isValidIPAddress(ip_str)) {
        return ip;
    }
    sscanf(ip_str, "%hhu.%hhu.%hhu.%hhu", 
        &ip.octet[0], &ip.octet[1], &ip.octet[2], &ip.octet[3]);
    return ip;
}

bool isIPInRange(IPAddress ip, IPRange range) {
    for (int i = 0; i < 4; i++) {
        if (ip.octet[i] < range.start.octet[i] || ip.octet[i] > range.end.octet[i]) {
            return false;
        }
        if (ip.octet[i] != range.start.octet[i]) {
            break;
        }
    }
    return true;
}

bool isPortInRange(unsigned short port, PortRange range) {
    return port >= range.start && port <= range.end;
}



IPRange parseIPRange(const char* ip_str, bool* isValid) {
    IPRange range = {{{0}}, {{0}}, 0}; 
    *isValid = true;
    char* dash = strchr(ip_str, '-');
    
    if (dash) {
        char start_ip[16];
        char end_ip[16];
        int len = dash - ip_str;
        strncpy(start_ip, ip_str, len);
        start_ip[len] = '\0';
        strcpy(end_ip, dash + 1);
        
        
        if (!isValidIPAddress(start_ip) || !isValidIPAddress(end_ip)) {
            *isValid = false;
            return range;
        }
        
        range.start = parseIPAddress(start_ip);
        range.end = parseIPAddress(end_ip);
        range.isRange = 1;
    } else {
        if (!isValidIPAddress(ip_str)) {
            *isValid = false;
            return range;
        }
        range.start = parseIPAddress(ip_str);
        range.end = range.start;
        range.isRange = 0;
    }
    
    return range;
}

PortRange parsePortRange(const char* port_str) {
    PortRange range;
    char* dash = strchr(port_str, '-');
    char* endptr;
    
    if (dash) {

        char start_port[6];
        char end_port[6];
        int len = dash - port_str;
        strncpy(start_port, port_str, len);
        start_port[len] = '\0';
        strcpy(end_port, dash + 1);

        int start_int = strtoul(start_port, &endptr, 10);
        int end_int = strtoul(end_port, &endptr, 10);

        if (start_int < 1 || start_int > 65535 || end_int < 1 || end_int > 65535){
            range.isRange = -1;
            return range;
        }
        range.start = (unsigned short)start_int;
        range.end = (unsigned short)end_int;
        range.isRange = 1;
    } else {
        int port_int = strtoul(port_str, &endptr, 10);

        if (port_int < 1 || port_int > 65535){
            range.isRange = -1;
            return range;
        }

        range.start = (unsigned short)port_int;
        range.end = range.start;
        range.isRange = 0;
    }
    
    return range;
}

void AddRequest(Request* head, char command[]) {
    Request* current = head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = malloc(sizeof(Request));
    strcpy(current->next->command, command);
    current->next->next = NULL;

}

void AddQuery(Rule* rule, IPAddress ipAddress, unsigned short port) {
    Query* newQuery = (Query*)malloc(sizeof(Query));
    if (newQuery != NULL) {
        newQuery->ipAddress = ipAddress;
        newQuery->port = port;
        newQuery->matchedRule = rule;
        newQuery->next = rule->queries;
        rule->queries = newQuery;
    }
}

bool queryExists(Query* head, IPAddress ipAddress, unsigned short port) {
    Query* current = head;
    while (current != NULL) {
        if (memcmp(&current->ipAddress, &ipAddress, sizeof(IPAddress)) == 0 && current->port == port) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void PrintRequests(Request* head) {
    Request* current = head;
    while (current != NULL) {
        printf("%s\n", current->command);
        current = current->next;
    }
}

void PrintRules(Rule* head) {
    Rule* current = head;
    
    if (current != NULL) {
        current = current->next;
    }

    while (current != NULL) {
        printf("Rule: ");
        
        printf("%d.%d.%d.%d", 
            current->ipRange.start.octet[0],
            current->ipRange.start.octet[1],
            current->ipRange.start.octet[2],
            current->ipRange.start.octet[3]);
            
        if (current->ipRange.isRange) {
            printf("-%d.%d.%d.%d", 
                current->ipRange.end.octet[0],
                current->ipRange.end.octet[1],
                current->ipRange.end.octet[2],
                current->ipRange.end.octet[3]);
        }
        
        printf(" %d", current->portRange.start);
        if (current->portRange.isRange) {
            printf("-%d", current->portRange.end);
        }
        printf("\n");
        
        Query* queryCurrent = current->queries;
        while (queryCurrent != NULL) {
            printf("Query: %d.%d.%d.%d %d\n",
                queryCurrent->ipAddress.octet[0],
                queryCurrent->ipAddress.octet[1],
                queryCurrent->ipAddress.octet[2],
                queryCurrent->ipAddress.octet[3],
                queryCurrent->port);
            queryCurrent = queryCurrent->next;
        }
        
        current = current->next;
    }
}

void AddRule(char command[], Rule* head)
{
    char* token = strtok(command, " \t");  
    char* ip_str = NULL;
    char* port_str = NULL;
    int isAllow = 0;


    if (token != NULL) {
        if (token[0] != 'A' && token[0] != 'D') {
            printf("Invalid rule\n");
            return;
        }
        isAllow = (token[0] == 'A' || token[0] == 'a');
        ip_str = strtok(NULL, " \t");
        if (ip_str != NULL) {
            port_str = strtok(NULL, " \t");
        }
    }

    if (ip_str == NULL || port_str == NULL) {
        printf("Invalid rule\n");
        return;
    }

    Rule* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(Rule));
    if (current->next == NULL) {

        return;
    }

    Rule* new_rule = current->next;
    new_rule->next = NULL;
    new_rule->isAllow = isAllow;

    bool isValidIP = true;
    new_rule->ipRange = parseIPRange(ip_str, &isValidIP);
    if (!isValidIP) {
        printf("Invalid rule\n");
        free(new_rule);
        current->next = NULL;
        return;
    }

    new_rule->portRange = parsePortRange(port_str);
    
    if (new_rule->portRange.isRange == -1 || new_rule->portRange.start > 65535 || new_rule->portRange.end > 65535) {
        printf("Invalid rule\n");
        free(new_rule);
        current->next = NULL;
        return;
    }

    if (new_rule->portRange.isRange && 
        new_rule->portRange.end < new_rule->portRange.start) {
        printf("Invalid rule\n");
        free(new_rule);
        current->next = NULL;
        return;
    }

    if (isValidIP) {
        printf("Rule added\n");
    } else {
        printf("Invalid rule\n");
    }
}

bool areIPRangesEqual(IPRange range1, IPRange range2) {
    
    return memcmp(&range1.start, &range2.start, sizeof(IPAddress)) == 0 &&
           (!range1.isRange || memcmp(&range1.end, &range2.end, sizeof(IPAddress)) == 0) &&
           range1.isRange == range2.isRange;
}

bool arePortRangesEqual(PortRange range1, PortRange range2) {
    if (range1.isRange) printf("-%d", range1.end);
    if (range2.isRange) printf("-%d", range2.end);
    
    return range1.start == range2.start &&
           range1.end == range2.end &&
           range1.isRange == range2.isRange;
}

bool areRulesEqual(Rule* rule1, Rule* rule2) {
    if (!areIPRangesEqual(rule1->ipRange, rule2->ipRange)) {
        return false;
    }
    if (!arePortRangesEqual(rule1->portRange, rule2->portRange)) {
        return false;
    }
    return true;
}

void deleteQueriesForRule(Query* queryHead, Rule* rule) {
    Query* current = queryHead;
    Query* prev = NULL;
    
    while (current != NULL) {
        Query* next = current->next;
        
        if (current->matchedRule == rule) {
            if (prev == NULL) {
                queryHead = next;
            } else {
                prev->next = next;
            }
            free(current);
        } else {
            prev = current;
        }
        
        current = next;
    }
}

bool deleteRule(Rule* head, Rule* ruleToDelete, Query* queryHead) {
    if (head == NULL || head->next == NULL) return false;

    Rule* current = head->next;
    Rule* prev = head;
    
    while (current != NULL) {

        if (areRulesEqual(current, ruleToDelete)) {
            deleteQueriesForRule(queryHead, current);
            
            prev->next = current->next;
            free(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}

Rule* parseRule(const char* ruleStr, bool* isValid) {
    
    Rule* rule = malloc(sizeof(Rule));
    *isValid = true;
    
    while (*ruleStr && isspace(*ruleStr)) ruleStr++;
    
    if (*ruleStr != 'A' && *ruleStr != 'D') {
        *isValid = false;
        free(rule);
        return NULL;
    }
    
    rule->isAllow = (*ruleStr == 'A');
    ruleStr++;
    
    while (*ruleStr && isspace(*ruleStr)) ruleStr++;
    
    char ip_str[256] = {0};
    int i = 0;
    while (*ruleStr && !isspace(*ruleStr) && i < 255) {
        ip_str[i++] = *ruleStr++;
    }
    ip_str[i] = '\0';
    
    bool isValidIP = true;
    rule->ipRange = parseIPRange(ip_str, &isValidIP);
    if (!isValidIP) {
        *isValid = false;
        free(rule);
        return NULL;
    }
    
    while (*ruleStr && isspace(*ruleStr)) ruleStr++;
    
    char port_str[256] = {0};
    i = 0;
    while (*ruleStr && !isspace(*ruleStr) && i < 255) {
        port_str[i++] = *ruleStr++;
    }
    port_str[i] = '\0';
    
    rule->portRange = parsePortRange(port_str);
    if (rule->portRange.isRange == -1 || rule->portRange.start > 65535 || rule->portRange.end > 65535) {
        *isValid = false;
        free(rule);
        return NULL;
    }
    
    rule->next = NULL;
    return rule;
}



Rule* isConnectionAllowed(Rule* rules, IPAddress ip, unsigned short port) {
    Rule* current = rules;
    Rule* firstAllowRule = NULL;
    bool denyFound = false;

    if (current != NULL) {
        current = current->next;
    }

    while (current != NULL) {
        if (isIPInRange(ip, current->ipRange) && isPortInRange(port, current->portRange)) {
            if (current->isAllow) {
                if (firstAllowRule == NULL) {
                    firstAllowRule = current;
                }
                if (!queryExists(current->queries, ip, port)) {
                    AddQuery(current, ip, port);
                    return firstAllowRule;
                }
            } else {
                denyFound = true;
                return NULL;
                
            }
        }
        current = current->next;
    }

    if (denyFound) {
        return NULL;
    }

    return NULL;
}

void HandleRequest(char command[], Request* requests, Rule* rules, Query* queries)
{
    AddRequest(requests, command);

    if (command[0] == 'R') {
        PrintRequests(requests);
    }
    else if (command[0] == 'A') {
        AddRule(command, rules);
    }
    else if (command[0] == 'C' && command[1] == ' ') {
        char ip_str[16];
        unsigned short port;

        if (sscanf(command + 2, "%15s %hu", ip_str, &port) != 2) {
            printf("Illegal IP address or port specified\n");
            return;
        }

        IPAddress ip = parseIPAddress(ip_str);

        if (isValidIPAddress(ip_str) && port <= 65535) {
            Rule* matchedRule = isConnectionAllowed(rules, ip, port);
            if (matchedRule != NULL) {
                printf("Connection accepted\n");
            } else {
                printf("Connection rejected\n");
                
            }
        } else {
            printf("Illegal IP address or port specified\n");
        }
    }
    else if (command[0] == 'D' && command[1] == ' ') {
        bool isValid = true;
        char fullCommand[256];
        snprintf(fullCommand, sizeof(fullCommand), "D %s", command + 2);
        Rule* ruleToDelete = parseRule(fullCommand, &isValid);

        if (deleteRule(rules, ruleToDelete, queries)) {
            printf("Rule deleted\n");
        } else {
            printf("Rule not found\n");
        }

        if (deleteRule(rules, ruleToDelete, queries)) {
            printf("Rule deleted\n");
        } else {
            printf("Rule not found\n");
        }
        free(ruleToDelete);
        return;
    }
    else if (command[0] == 'L') {
        PrintRules(rules);
    }
    else {
        printf("Illegal request\n");
    }
}

void InteractiveMode()
{
    Request* requests = malloc(sizeof(Request));
    Rule* rules = malloc(sizeof(Rule));
    Query* queries = malloc(sizeof(Query));

    requests->next = NULL;
    rules->next = NULL;
    queries->next = NULL;

    char command[256];
    char *fgets_result;

    while (1) {
        fgets_result = fgets(command, sizeof(command), stdin);
        
        if (fgets_result == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = 0;

        if (command[0] == 'Q') {
            break;
        }

        if (strlen(command) > 0) {
            HandleRequest(command, requests, rules, queries);
            fflush(stdout);
        }
    }

    free(requests);
    free(rules);
    free(queries);
}

void* handle_client(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int new_socket = args->socket;
    char buffer[1024] = {0};
    char response[1024] = {0};

    while(1) {

        memset(buffer, 0, sizeof(buffer));
        memset(response, 0, sizeof(response));
        
        int valread = read(new_socket, buffer, 1024);
        if (valread <= 0) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "Q") == 0) {
            break;
        }

        FILE *temp = tmpfile();
        FILE *old_stdout = stdout;
        stdout = temp;

        HandleRequest(buffer, args->requests, args->rules, args->queries);

        fflush(stdout);
        stdout = old_stdout;
        rewind(temp);
        fread(response, sizeof(char), sizeof(response)-1, temp);
        fclose(temp);

        send(new_socket, response, strlen(response), 0);
    }

    close(new_socket);
    free(arg);
    return NULL;
}

void ServerMode(int port) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    Request* requests = malloc(sizeof(Request));
    Rule* rules = malloc(sizeof(Rule));
    Query* queries = malloc(sizeof(Query));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        exit(EXIT_FAILURE);
    }


    if (listen(server_fd, 3) < 0) {
        exit(EXIT_FAILURE);
    }


    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            continue;
        }

        ThreadArgs* args = malloc(sizeof(ThreadArgs));
        args->socket = new_socket;
        args->requests = requests;
        args->rules = rules;
        args->queries = queries;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)args) != 0) {
            free(args);
            close(new_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
    free(requests);
    free(rules);
    free(queries);
}


int main (int argc, char ** argv) {


    CmdArg cmd;
    cmd = ParseCmdLine(argc, argv, &cmd);
    if (cmd.isInteractive)
    {
        InteractiveMode();
    }
    else
    {
        ServerMode(cmd.port);
    }


    return 0;
}
