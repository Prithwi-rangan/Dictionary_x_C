#define _POSIX_C_SOURCE 200809L
#include "wordmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RESULTS_FILE "data/activity_results.tsv"

int save_activity_result(const char *activity, int correct, int total,
                         int points, unsigned long duration_seconds) {
    if (!activity || !activity[0] || total <= 0) return 0;
    char safe_activity[64];
    snprintf(safe_activity, sizeof(safe_activity), "%s", activity);
    for (char *p = safe_activity; *p; ++p)
        if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char timestamp[32] = "unknown-time";
    if (local) strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local);
    FILE *file = fopen(RESULTS_FILE, "a");
    if (!file) { perror(RESULTS_FILE); return 0; }
    double accuracy = 100.0 * correct / total;
    int ok = fprintf(file, "%s\t%s\t%d\t%d\t%d\t%lu\t%.2f\n",
                     timestamp, safe_activity, correct, total, points,
                     duration_seconds, accuracy) >= 0;
    if (fclose(file) != 0) ok = 0;
    if (ok) puts("Activity result saved.");
    else fprintf(stderr, "Could not save the activity result.\n");
    return ok;
}

int show_activity_results(void) {
    FILE *file = fopen(RESULTS_FILE, "r");
    if (!file) { puts("No activity results have been saved yet."); return 0; }
    puts("\nDate and time        Activity              Score    Accuracy   Points   Time");
    puts("--------------------------------------------------------------------------");
    char *line = NULL;
    size_t capacity = 0;
    int shown = 0;
    char date[32], activity[64];
    int correct, total, points;
    unsigned long duration;
    double accuracy;
    while (getline(&line, &capacity, file) != -1) {
        if (sscanf(line, "%31[^\t]\t%63[^\t]\t%d\t%d\t%d\t%lu\t%lf",
                   date, activity, &correct, &total, &points, &duration, &accuracy) == 7) {
            printf("%-20s %-21s %3d/%-3d %7.2f%% %7d %3lu:%02lu\n",
                   date, activity, correct, total, accuracy, points,
                   duration / 60, duration % 60);
            ++shown;
        }
    }
    free(line);
    fclose(file);
    if (!shown) puts("No readable activity results were found.");
    return shown > 0;
}

int show_performance_statistics(void) {
    FILE *file = fopen(RESULTS_FILE, "r");
    if (!file) { puts("Complete a vocabulary activity to start your progress history."); return 0; }
    char *line = NULL;
    size_t capacity = 0;
    size_t activities = 0;
    long long correct_total = 0, question_total = 0;
    double best_accuracy = 0.0;
    int highest_points = -1;
    char best_activity[64] = "";
    char highest_activity[64] = "";
    char date[32], activity[64];
    int correct, total, points;
    unsigned long duration;
    double accuracy;
    while (getline(&line, &capacity, file) != -1) {
        if (sscanf(line, "%31[^\t]\t%63[^\t]\t%d\t%d\t%d\t%lu\t%lf",
                   date, activity, &correct, &total, &points, &duration, &accuracy) != 7 || total <= 0) continue;
        (void)points; (void)duration;
        ++activities;
        correct_total += correct;
        question_total += total;
        if (accuracy > best_accuracy) {
            best_accuracy = accuracy;
            snprintf(best_activity, sizeof(best_activity), "%s", activity);
        }
        if (points > highest_points) {
            highest_points = points;
            snprintf(highest_activity, sizeof(highest_activity), "%s", activity);
        }
    }
    free(line);
    fclose(file);
    if (!activities || !question_total) { puts("No readable results are available."); return 0; }
    printf("\nActivities completed: %zu\nQuestions answered:   %lld\nOverall accuracy:     %.2f%%\nBest accuracy:        %.2f%% (%s)\nHighest score:        %d points (%s)\n",
           activities, question_total, 100.0 * (double)correct_total / (double)question_total,
           best_accuracy, best_activity, highest_points, highest_activity);
    return 1;
}
