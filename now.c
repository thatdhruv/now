//
// now.c - a minimal todo manager for the command line
// fast, simple, and unapologetically written in C
//

#define _GNU_SOURCE

#include <regex.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DESC     512
#define MAX_TASKS    1024

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_BOLD   "\033[1m"
#define COLOR_DIM    "\033[2m"
#define COLOR_ITALIC "\033[3m"

char nowfile[1024];

typedef struct {
	int id;
	char description[MAX_DESC];
	time_t created_at;
	time_t completed_at;
	time_t due_at;
	int done;
} Task;

void print_usage(void) {
	printf("Usage: now <command> [options]\n\n");
	printf("Commands:\n");
	printf("  add \"task 1\" [\"task 2\" ...]  Add one or more tasks\n");
	printf("  done <id> [id ...]           Mark one or more tasks as done\n");
	printf("  remove <id> [id ...]         Remove one or more tasks\n");
	printf("  list [options]               List tasks\n");
	printf("  search \"keyword\"             Search tasks by keyword\n");
	printf("  stats                        Show task statistics\n\n");

	printf("Options for list:\n");
	printf("  --raw                        Display raw task descriptions without formatting\n");
	printf("  --due                        Show only tasks with a due date\n");
	printf("  --completed                  Show only completed tasks\n");
	printf("  --pending                    Show only pending tasks\n");
	printf("  --sort=created               Sort tasks by creation date\n");
	printf("  --sort=completed             Sort tasks by completion date\n");
	printf("  --sort=due                   Sort tasks by due date\n\n");

	printf("Notes:\n");
	printf("  Specify due dates in task descriptions by using the @due:YYYY-MM-DD format\n");
	printf("  Basic markdown is supported when adding task descriptions. Use:\n");
	printf("    Single asterisks (*) to %semphasize%s the text\n", COLOR_ITALIC, COLOR_RESET);
	printf("    Double asterisks (**) to make the text %sbold%s\n", COLOR_BOLD, COLOR_RESET);
	printf("    Double hashtags (##) to %shighlight%s the text\n\n", COLOR_CYAN, COLOR_RESET);
}

int load_tasks(Task *tasks, int max_tasks) {
	FILE *file = fopen(nowfile, "rb");
	if (!file)
		return 0;

	int task_count = fread(tasks, sizeof(Task), max_tasks, file);
	fclose(file);
	return task_count;
}

void save_tasks(Task *tasks, int task_count) {
	FILE *file = fopen(nowfile, "wb");
	if (!file) {
		perror("failed to save tasks!");
		return;
	}

	fwrite(tasks, sizeof(Task), task_count, file);
	fclose(file);
}

void format_time(time_t raw, char *buffer, size_t size) {
	struct tm *timeinfo = localtime(&raw);
	strftime(buffer, size, "%Y-%m-%d", timeinfo);
}

void print_markdown(const char *description) {
	const char *p = description;

	while (*p) {
		if (*p == '*' && *(p + 1) == '*') {
			printf(COLOR_BOLD);
			p += 2;
			while (*p && (*p != '*' || *(p + 1) != '*'))
				putchar(*p++);
			p += 2;
			printf(COLOR_RESET);
		} else if (*p == '*' && *(p + 1) != '*') {
			printf(COLOR_ITALIC);
			p++;
			while (*p && *p != '*')
				putchar(*p++);
			p++;
			printf(COLOR_RESET);
		} else if (*p == '#' && *(p + 1) == '#') {
			printf(COLOR_CYAN);
			p += 2;
			while (*p && *p != '#')
				putchar(*p++);
			p += 2;
			printf(COLOR_RESET);
		} else if (strncmp(p, "@due:", 5) == 0) {
			if (p > description && *(p - 1) == ' ')
				p--;
			p += 5;
			while (*p && *p != ' ' && *p != '\t')
				p++;
			if (*p == ' ' || *p == '\t')
				p++;
		} else
			putchar(*p++);
	}
}

void parse_due_date(const char *desc, time_t *due_at) {
	regex_t regex;
	regmatch_t matches[2];
	const char *pattern = "@due:([0-9]{4}-[0-9]{2}-[0-9]{2})";

	regcomp(&regex, pattern, REG_EXTENDED);
	if (regexec(&regex, desc, 2, matches, 0) == 0) {
		char date_str[11];
		strncpy(date_str, desc + matches[1].rm_so, 10);
		date_str[10] = '\0';

		struct tm tm = {0};
		strptime(date_str, "%Y-%m-%d", &tm);
		*due_at = mktime(&tm);
	} else
		*due_at = 0;

	regfree(&regex);
}

void print_task(const Task *t, int raw) {
	char created[20], completed[20], due[20];

	format_time(t->created_at, created, sizeof(created));
	if (t->completed_at)
		format_time(t->completed_at, completed, sizeof(completed));
	if (t->due_at)
		format_time(t->due_at, due, sizeof(due));

	time_t now = time(NULL);
	int overdue = (!t->done && t->due_at && now > t->due_at);
	const char *status, *color;

	if (t->done) {
		status = "[x]";
		color = COLOR_GREEN;
	} else if (overdue) {
		status = "[*]";
		color = COLOR_RED;
	} else {
		status = "[ ]";
		color = COLOR_RESET;
	}

	int age = (int)((now - t->created_at) / 86400);

	printf("%s%3d.%s %s%s%s ", COLOR_YELLOW, t->id, COLOR_RESET, color, status, COLOR_RESET);

	if (raw)
		printf("%s", t->description);
	else
		print_markdown(t->description);

	printf(" %s(%dd)%s", COLOR_DIM, age, COLOR_RESET);
	printf(" %sadded:%s %s", COLOR_DIM, COLOR_RESET, created);
	if (t->done)
		printf(", %sdone:%s %s", COLOR_GREEN, COLOR_RESET, completed);
	if (t->due_at)
		printf(", %s%s:%s %s", COLOR_RED, overdue ? "overdue" : "due", COLOR_RESET, due);
	printf("\n");
}

int cmp_created(const void *a, const void *b) {
	const Task *x = a, *y = b;
	return (x->created_at > y->created_at) - (x->created_at < y->created_at);
}

int cmp_completed(const void *a, const void *b) {
	const Task *x = a, *y = b;
	if (!x->done && !y->done)
		return 0;
	if (!x->done)
		return 1;
	if (!y->done)
		return -1;
	return (x->completed_at > y->completed_at) - (x->completed_at < y->completed_at);
}

int cmp_due(const void *a, const void *b) {
	const Task *x = a, *y = b;
	if (!x->due_at && !y->due_at)
		return 0;
	if (!x->due_at)
		return 1;
	if (!y->due_at)
		return -1;
	return (x->due_at > y->due_at) - (x->due_at < y->due_at);
}

int main(int argc, char *argv[]) {
	snprintf(nowfile, sizeof(nowfile), "%s/.nowfile", getenv("HOME"));

	if (argc < 2) {
		print_usage();
		return 1;
	}

	if (strcmp(argv[1], "add") == 0) {
		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);

		for (int i = 2; i < argc; i++) {
			if (task_count >= MAX_TASKS) {
				printf("maximum task limit reached.\n");
				break;
			}

			Task t;
			t.id = (task_count > 0) ? tasks[task_count - 1].id + 1 : 1;
			strncpy(t.description, argv[i], MAX_DESC);
			t.created_at = time(NULL);
			t.completed_at = 0;
			t.due_at = 0;
			t.done = 0;
			parse_due_date(t.description, &t.due_at);

			tasks[task_count++] = t;
			printf("added task #%d: ", t.id);
			print_markdown(t.description);
			printf("\n");
		}
		printf("\n");

		save_tasks(tasks, task_count);
		return 0;
	}

	if (strcmp(argv[1], "done") == 0) {
		if (argc < 3) {
			printf("Usage: now done <id> ...\n");
			return 1;
		}

		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);
		if (!task_count) {
			printf("no tasks found.\n");
			return 0;
		}

		for (int i = 2; i < argc; i++) {
			int task_id = atoi(argv[i]), found = 0;

			for (int j = 0; j < task_count; j++) {
				if (tasks[j].id == task_id) {
					tasks[j].done = 1;
					tasks[j].completed_at = time(NULL);
					printf("task #%d marked as done.\n", task_id);
					found = 1;
					break;
				}
			}

			if (!found)
				printf("task #%d not found.\n", task_id);
		}
		printf("\n");

		save_tasks(tasks, task_count);
		return 0;
	}

	if (strcmp(argv[1], "remove") == 0) {
		if (argc < 3) {
			printf("Usage: now remove <id> ...\n");
			return 1;
		}

		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);
		if (!task_count) {
			printf("no tasks found.\n");
			return 0;
		}

		for (int i = 2; i < argc; i++) {
			int task_id = atoi(argv[i]), found = 0;

			for (int j = 0; j < task_count; j++) {
				if (tasks[j].id == task_id) {
					for (int k = j; k < task_count - 1; k++)
						tasks[k] = tasks[k + 1];
					task_count--;
					printf("task #%d removed.\n", task_id);
					found = 1;
					break;
				}
			}

			if (!found)
				printf("task #%d not found.\n", task_id);
		}
		printf("\n");

		for (int i = 0; i < task_count; i++)
			tasks[i].id = i + 1;

		save_tasks(tasks, task_count);
		return 0;
	}

	if (strcmp(argv[1], "search") == 0) {
		if (argc < 3) {
			printf("Usage: now search \"keyword\"\n");
			return 1;
		}

		const char *needle = argv[2];
		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);
		if (!task_count) {
			printf("no tasks found.\n");
			return 0;
		}

		int found = 0;
		for (int i = 0; i < task_count; i++) {
			if (strcasestr(tasks[i].description, needle)) {
				print_task(&tasks[i], 0);
				found = 1;
			}
		}

		if (!found)
			printf("no tasks contained \"%s\".\n", needle);
		printf("\n");

		return 0;
	}

	if (strcmp(argv[1], "list") == 0) {
		int raw = 0, due = 0, completed = 0, pending = 0;

		enum {
			SORT_NONE,
			SORT_CREATED,
			SORT_COMPLETED,
			SORT_DUE
		} sort_key = SORT_NONE;

		for (int i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--raw") == 0)
				raw = 1;
			else if (strcmp(argv[i], "--due") == 0)
				due = 1;
			else if (strcmp(argv[i], "--completed") == 0)
				completed = 1;
			else if (strcmp(argv[i], "--pending") == 0)
				pending = 1;
			else if (strncmp(argv[i], "--sort=", 7) == 0) {
				const char *key = argv[i] + 7;

				if (!strcmp(key, "created"))
					sort_key = SORT_CREATED;
				else if (!strcmp(key, "completed"))
					sort_key = SORT_COMPLETED;
				else if (!strcmp(key, "due"))
					sort_key = SORT_DUE;
				else {
					printf("unknown sort key.\n");
					return 1;
				}
			}
		}

		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);
		if (!task_count) {
			printf("no tasks found.\n");
			return 0;
		}

		if (sort_key == SORT_CREATED)
			qsort(tasks, task_count, sizeof(Task), cmp_created);
		else if (sort_key == SORT_COMPLETED)
			qsort(tasks, task_count, sizeof(Task), cmp_completed);
		else if (sort_key == SORT_DUE)
			qsort(tasks, task_count, sizeof(Task), cmp_due);

		printf("%sYour tasks:%s\n", COLOR_BOLD, COLOR_RESET);
		for (int i = 0; i < task_count; i++) {
			if (due && (!tasks[i].due_at || tasks[i].done))
				continue;
			if (completed && !tasks[i].done)
				continue;
			if (pending && tasks[i].done)
				continue;

			print_task(&tasks[i], raw);
		}
		printf("\n");

		return 0;
	}

	if (strcmp(argv[1], "stats") == 0) {
		Task tasks[MAX_TASKS];
		int task_count = load_tasks(tasks, MAX_TASKS);
		if (!task_count) {
			printf("no tasks found.\n");
			return 0;
		}

		int done = 0, overdue = 0;
		time_t now = time(NULL), next_due = 0;
		for (int i = 0; i < task_count; i++) {
			if (tasks[i].done)
				done++;
			else if (tasks[i].due_at && tasks[i].due_at < now)
				overdue++;
			if (tasks[i].due_at && (!next_due || tasks[i].due_at < next_due) && !tasks[i].done)
				next_due = tasks[i].due_at;
		}

		char buffer[20];
		if (next_due)
			format_time(next_due, buffer, sizeof(buffer));

        printf("%sYour task statistics:%s\n", COLOR_BOLD, COLOR_RESET);
		printf("Total tasks: %d\n", task_count);
		printf("Completed:   %d\n", done);
		printf("Pending:     %d\n", task_count - done);
		printf("Overdue:     %d\n", overdue);
		printf("Next due:    %s\n", next_due ? buffer : "none");
		printf("\n");

		return 0;
	}

	print_usage();
	return 1;
}
