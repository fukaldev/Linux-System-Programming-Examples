#include "helper.h"

void calculateMetric(double* metrics, double* mean, double* dev, int index)
{
        *mean = 0;
        *dev = 0;
        double sum = 0;

        for(int i = 0; i < index; i++)
        {
                *mean += metrics[i];
        }

        for(int i = 0; i < index; i++)
        {
                sum += pow(metrics[i]-(*mean), 2);
        }

        *dev = sqrt(sum/index);
}

void calculateAndCreateLine(char* buffer, char* line)
{
        int sum_x = 0, sum_y = 0, sum_x2 = 0, sum_xy = 0;
        double a, b;
        char convertBuffer[10];
        for(int i = 0; i < 20; i = i+2)
        {
                sum_x += (int) buffer[i];
                sum_y += (int) buffer[i+1];
                sum_x2 += (int) buffer[i] * (int) buffer[i];
                sum_xy +=  (int)buffer[i]*(int) buffer[i+1];
        } 

        a = ((double)((10*sum_xy)-(sum_y*sum_x)))/((10*sum_x2)-(sum_x*sum_x));
        b = ((double)(sum_y-(a*sum_x)))/10;

        for(int i = 0; i < 20; i = i+2)
        {
                strcat(line, "(");
                sprintf(convertBuffer, "%d", (int)buffer[i]);
                strcat(line, convertBuffer);
                strcat(line, ",");
                sprintf(convertBuffer, "%d", (int)buffer[i+1]);
                strcat(line, convertBuffer);
                strcat(line, "),");
        } 

        sprintf(convertBuffer, "%.3f", a);
        strcat(line, convertBuffer);
        strcat(line, "x+");
        sprintf(convertBuffer, "%.3f", b);
        strcat(line, convertBuffer);
        strcat(line, "\n");   
}

static double calculateMAE(double predictions[], point coordinates[])
{
        double total = 0;
        for(int i = 0; i < 10; i++)
        {
                total += abs(coordinates[i].y - predictions[i]);
        }
        return total/10;
}

static double calculateMSE(double predictions[], point coordinates[])
{
        double total = 0;
        for(int i = 0; i < 10; i++)
        {
                total += pow(coordinates[i].y - predictions[i], 2);
        }
        return total/10;
}

static double calculateRMSE(double predictions[], point coordinates[])
{
        double total = 0;
        for(int i = 0; i < 10; i++)
        {
                total += pow(coordinates[i].y - predictions[i], 2);
        }
        return sqrt(total/10);
}

static double calculatePrediction(double theta1, double theta0, int test_x)
{
        return (theta1*test_x)+theta0;
}

void processLine(char* line, char* errors, metrics* m)
{
        regression r;
        point coordinates[10];
        char numberBuffer[20];
        double predictions[10];
        double MAE, MSE, RMSE;
        int numberCounter;
        int j = 0;
        for(int i = 0; i < 10; i++)//Get 10 coordinates with their integer values
        {
                numberCounter = 0;
                while(1)
                {
                        if(line[j] == '(')
                        {
                                j++;
                                continue;
                        }       
                        else if(line[j] == ',')
                        {
                                coordinates[i].x = atoi(numberBuffer);
                                memset(numberBuffer, 0, sizeof(char)*20);
                                numberCounter = 0;
                        }
                        else if(line[j] == ')')
                        {
                                coordinates[i].y = atoi(numberBuffer);
                                memset(numberBuffer, 0, sizeof(char)*20);
                                j += 2;
                                break;
                        }
                        else
                        {
                                numberBuffer[numberCounter] = line[j];
                                numberCounter++;
                        }
                        j++;
                }
        }
        
        numberCounter = 0;
        for(int i = j; line[i] != '\n'; i++)//Get the regression line
        {
                if(line[i] == 'x')
                {
                        r.theta1 = strtod(numberBuffer, NULL);
                        memset(numberBuffer, 0, sizeof(char)*10);
                        numberCounter = 0;
                        i++;
                } 
                else if(line[i+1] == '\n')
                {
                        r.theta0 = strtod(numberBuffer,NULL);
                        memset(numberBuffer, 0, sizeof(char)*10);
                        numberCounter = 0;
                }
                else
                {
                        numberBuffer[numberCounter] = line[i];
                        numberCounter++;
                }
        }

        for(int i = 0; i < 10; i++)
        {
                predictions[i] = calculatePrediction(r.theta1, r.theta0, coordinates[i].x);
        }
        MAE = calculateMAE(predictions, coordinates);
        MSE = calculateMSE(predictions, coordinates);
        RMSE = calculateRMSE(predictions, coordinates);

        m->mae = MAE;
        m->mse = MSE;
        m->rmse = RMSE;

        strcat(errors, ",");
        sprintf(numberBuffer, "%.3f", MAE);
        strcat(errors, numberBuffer);
        strcat(errors, ",");
        sprintf(numberBuffer, "%.3f", MSE);
        strcat(errors, numberBuffer);
        strcat(errors, ",");
        sprintf(numberBuffer, "%.3f", RMSE);
        strcat(errors, numberBuffer);
        strcat(errors, "\n");
}

int deleteFirstLine(int fd)
{
        int status, totalByte = 0;
        char tempBuffer[120];
        off_t current = lseek(fd, 0, SEEK_CUR);
        off_t place = lseek(fd, 0, SEEK_SET);
        while (1)
        {
                lseek(fd, current, SEEK_SET);
                status = read(fd, tempBuffer, 120);

                totalByte += status;
                if (status == 0)
                        break;
                current = lseek(fd, 0, SEEK_CUR);
                lseek(fd, place, SEEK_SET);
                if (write(fd, tempBuffer, status) != status)
                        return -1;
                place = lseek(fd, 0, SEEK_CUR);
        }
        ftruncate(fd, totalByte);
        return 0;
}

void p1Done(int signo)
{
        printf("P1 done\n");
        existence = 0;
}

void justReturn(int signo)
{
}

void generalHandler(int signo)
{
}

void termProcess(int signo)
{
        if (remove(input) < 0)
        {
                fprintf(stderr, "remove 4: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
}