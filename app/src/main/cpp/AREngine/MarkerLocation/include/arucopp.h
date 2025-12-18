#pragma once


#include"opencv2/objdetect/aruco_detector.hpp"
#include"opencv2/objdetect/aruco_dictionary.hpp"
#include"opencv2/highgui.hpp"
#include"opencv2/calib3d.hpp"
#include"opencv2/imgproc.hpp"
#include<fstream>
#include<iostream>
#include<BFC/portable.h>
#include"CVX/core.h"
#include<CVX/vis.h>
#undef string_t
#include<nlohmann/json.hpp>
using namespace cv;

#define _ARUCOPP_ENABLE_UI 0

#if _ARUCOPP_ENABLE_UI
#include<CVX/gui.h>
#endif

class ArucoPP
{
public:
    struct Template
    {
        float boardWidth, boardHeight;

        struct Marker
        {
            int arucoId;
            std::vector<Point2f>  corners;
        };
        std::vector<Marker>  markers;

        struct Circle
        {
            Point2f center;
            Rect2f rect;
        };
        std::vector<Circle>  circles;
    public:
        void save(const std::string& file) const
        {
            nlohmann::json jmarkers, jcircles;
            for (auto& m : markers)
            {
                nlohmann::json jm;
                jm["arucoId"] = m.arucoId;
                nlohmann::json jcorners;
                for (auto& c : m.corners)
                {
                    nlohmann::json jc;
                    jc["x"] = c.x;
                    jc["y"] = c.y;
                    jcorners.push_back(jc);
                }
                jm["corners"] = jcorners;
                jmarkers.push_back(jm);
            }
            for (auto& c : circles)
            {
                nlohmann::json jc;
                jc["center"] = { c.center.x, c.center.y };
                jc["rect"] = { c.rect.x, c.rect.y, c.rect.width, c.rect.height };
                jcircles.push_back(jc);
            }
            nlohmann::json j;
            j["boardWidth"] = boardWidth;
            j["boardHeight"] = boardHeight;
            j["markers"] = jmarkers;
            j["circles"] = jcircles;
            std::ofstream ofs(file);
            ofs << j.dump(4);
        }
        void load(const std::string& file)
        {
            nlohmann::json j;
            std::ifstream ifs(file);
            ifs >> j;
            boardWidth = j.at("boardWidth").get<float>();
            boardHeight = j.at("boardHeight").get<float>();
            markers.clear();
            for (auto& jm : j.at("markers"))
            {
                Marker m;
                m.arucoId = jm.at("arucoId").get<int>();
                for (auto& jc : jm.at("corners"))
                {
                    Point2f c;
                    c.x = jc.at("x").get<float>();
                    c.y = jc.at("y").get<float>();
                    m.corners.push_back(c);
                }
                markers.push_back(m);
            }
            circles.clear();
            for (auto& jc : j.at("circles"))
            {
                Circle c;
                c.center.x = jc.at("center").at(0).get<float>();
                c.center.y = jc.at("center").at(1).get<float>();
                c.rect.x = jc.at("rect").at(0).get<float>();
                c.rect.y = jc.at("rect").at(1).get<float>();
                c.rect.width = jc.at("rect").at(2).get<float>();
                c.rect.height = jc.at("rect").at(3).get<float>();
                circles.push_back(c);
            }
        }
    };

    static bool _getTemplate(Template &templ, Mat rimg, double boardWidth, double boardHeight, int nCircles, int circleThreshold=128)
    {
        double vscale=800.0/__max(rimg.cols, rimg.rows);

        cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_50);
        cv::aruco::ArucoDetector detector;
        detector.setDictionary(dictionary);

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        detector.detectMarkers(rimg, corners, ids); //检测markers

        if (ids.size() != 1)
            return false;

        auto get3DPoint = [&](const cv::Point2f& p) {
            double x= double(p.x)/rimg.cols*boardWidth - boardWidth / 2;
            double y= double(p.y)/rimg.rows*boardHeight - boardHeight / 2;
            return Point2f(x, y);
        };

        Template::Marker marker;
        marker.arucoId = ids[0];
        marker.corners.clear();
        for (auto& c : corners[0])
        {
            marker.corners.push_back(get3DPoint(c));
        }
        templ.markers.clear();
        templ.markers.push_back(marker);


        templ.circles.clear();
        Mat mask;
        cv::threshold(rimg, mask, circleThreshold, 255, cv::THRESH_BINARY_INV);

        {
            Rect roi=cv::getBoundingBox2D(corners[0]);
            if(!roi.empty())
                mask(roi).setTo(0);
        }

        //imshow("mask", imscale(mask, 0.5));

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        {
            std::vector<std::pair<int, int>> vidx;
            for (int i = 0; i < contours.size(); i++)
                vidx.push_back(std::make_pair(i, cv::contourArea(contours[i])));
            std::sort(vidx.begin(), vidx.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second > b.second; });

            int nmax = __min(nCircles, (int)vidx.size());
            std::vector<std::vector<cv::Point>> tempContours(nmax);
            for (int i = 0; i < nmax; i++)
            {
                int idx = vidx[i].first;
                tempContours[i].swap( contours[idx] );
            }
            contours.swap(tempContours);
        }

        Mat dimg = cv::convertBGRChannels(rimg, 3);
        std::vector<cv::RotatedRect> circles;
        {
            for(auto &c : contours)
            {
                Rect2f roi=cv::getBoundingBox2D(c);
                {
                    cv::Point2f lt=get3DPoint(Point2f(roi.x, roi.y));
                    cv::Point2f rb=get3DPoint(Point2f(roi.x + roi.width, roi.y + roi.height));
                    roi=Rect2f(lt, rb);
                }
                auto r=cv::fitEllipse(c);
                circles.push_back(r);
                templ.circles.push_back({get3DPoint(Point2f(r.center.x, r.center.y)), roi});

                cv::ellipse(dimg, r, cv::Scalar(0, 255, 0), int(2.0/vscale), CV_AA);
            }
        }
#if _ARUCOPP_ENABLE_UI
        //cv::drawContours(dimg, contours, -1, cv::Scalar(255, 0, 0), 3, 8);
        imshow("contours", imscale(dimg, vscale));
#endif
        templ.boardWidth = boardWidth;
        templ.boardHeight = boardHeight;

        return true;
    }

    static void genTemplate(const std::string& dir)
    {
        std::string cfgFile = dir + "/config.txt";
        std::ifstream ifs(cfgFile);
        if (!ifs.is_open())
        {
            std::cout << "Cannot open config file: " << cfgFile << std::endl;
            return;
        }
        std::string str;
        double boardWidth, boardHeight;
        int nCircles;
        ifs>>str>>boardWidth>>boardHeight;
        ifs>>str>>nCircles;

        while (ifs >> str)
        {
            auto imgFile=dir+"/"+str;
            if (ff::pathExist(imgFile))
            {
                std::vector<Point2f> boardCorners;
                Point2f pt;
                char sep;
                while (ifs >> pt.x >> pt.y >> sep)
                    boardCorners.push_back(pt);
                if (boardCorners.size() < 4)
                    continue;
                Mat img = imread(imgFile, IMREAD_GRAYSCALE);
                CV_Assert(!img.empty());

                double reso = (img.cols + img.rows) / (boardWidth + boardHeight);
                Size boardSize(int(boardWidth * reso), int(boardHeight * reso));
                const int bw = 0;
                std::vector<Point2f> rectifiedBoardCorners = {
                        Point2f(bw, bw),
                        Point2f(boardSize.width - bw, bw),
                        Point2f(boardSize.width - bw, boardSize.height - bw),
                        Point2f(bw, boardSize.height - bw)
                };

                Mat H=cv::findHomography(boardCorners, rectifiedBoardCorners, 0);
                Mat rimg;
                cv::warpPerspective(img, rimg, H, boardSize);

                //cv::imshow("rimg", imscale(rimg, 0.5));

                Template templ;
                _getTemplate(templ, rimg, boardWidth, boardHeight, nCircles);

                templ.save(dir + "/" + "templ.json");

                //cv::waitKey();
            }
        }
    }

private:
    Template _templ;
    cv::aruco::Dictionary _dictionary;
    cv::aruco::ArucoDetector _detector;
public:
    void loadTemplate(const std::string& file)
    {
        _templ.load(file);

        _dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_50);
        _detector.setDictionary(_dictionary);
    }

    int _estimateThreshold(const Mat1b& img, const std::vector<cv::Rect>& rois)
    {
        int vhist[256] = { 0 };
        for (auto& roi : rois)
        {
            if (roi.empty())
                continue;

            Mat1b cimg = img(roi);
            for_each_1(DWHN1(cimg), [&vhist](uchar v) {
                vhist[v]++;
            });
        }
        cv::Mat _hist(1, 256, CV_32SC1, vhist);
        cv::boxFilter(_hist, _hist, -1, Size(15, 1));
        CV_Assert(_hist.type() == CV_32SC1 && _hist.data == (uchar*)vhist);

        auto calcMean = [&vhist](int start, int end, int *stddev=nullptr)
        {
            double sum = 0, n = 0;
            for (int i = start; i < end; i++)
            {
                sum += i * vhist[i];
                n += vhist[i];
            }
            double mean=sum/n;
            if (stddev)
            {
                double var=0;
                for (int i = start; i < end; i++)
                {
                    double diff = i - mean;
                    var += vhist[i] * diff * diff;
                }
                *stddev = (int)sqrt(var / n);
            }
            return (int)mean;
        };

        int mean = calcMean(0, 256);
        int upperMean = calcMean(mean, 256);
        int lowerStddev;
        int lowerMean = calcMean(0, mean, &lowerStddev);
        int lowerEnd=lowerMean+lowerStddev;

        mean = (upperMean + lowerMean) / 2;
        int T = lowerEnd + (mean - lowerEnd) / 2;
        //printf("lowerEnd=%d, T=%d\n", lowerEnd, T);
        return T;
    }

    bool detect(Mat img, const Matx33f &K, cv::OutputArray rvec, cv::OutputArray tvec)
    {
        if (img.channels() != 1)
            img = cv::convertBGRChannels(img, 1);
        //cv::GaussianBlur(img, img, Size(3, 3), 1.0);

        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        _detector.detectMarkers(img, corners, ids); //检测markers

        if(_templ.markers.size() != 1)
            return false;
        auto& marker = _templ.markers[0];

        for (int mi = 0; mi < ids.size(); mi++) {
            if (ids[mi] != marker.arucoId)
                continue;

            Mat H=cv::findHomography(marker.corners, corners[mi], 0);
            if (H.empty())
                continue;

            std::vector<cv::Point3f>  modelPoints;
            std::vector<cv::Point2f>  imagePoints;

            if (false)
            {
                imagePoints = corners[mi];
                for (auto& c : marker.corners)
                    modelPoints.push_back(Point3f(c.x, c.y, 0));

                if (cv::solvePnP(modelPoints, imagePoints, K, cv::Mat(), rvec, tvec))
                {
                    return true;
                }
                continue;
            }

#if _ARUCOPP_ENABLE_UI
            Mat dimg = cv::convertBGRChannels(img, 3);
            Mat dmask = Mat1b::zeros(img.size());

            aruco::drawDetectedMarkers(dimg, corners, ids);
#endif
            struct _DCircle
            {
                Rect croi;
            };

            std::vector<_DCircle> circles;
            circles.resize(_templ.circles.size());
            for (int i = 0; i < _templ.circles.size(); i++)
            {
                auto& c = _templ.circles[i];
                auto roi = c.rect;
                std::vector<cv::Point2f> roiCorners = {
                        Point2f(roi.x, roi.y),
                        Point2f(roi.x + roi.width, roi.y),
                        Point2f(roi.x + roi.width, roi.y + roi.height),
                        Point2f(roi.x, roi.y + roi.height)
                };
                cv::perspectiveTransform(roiCorners, roiCorners, H);
#if _ARUCOPP_ENABLE_UI
                //cv::polylines(dimg, { cvtPoint(roiCorners) }, true, Scalar(0, 255, 0), 2, CV_AA);
#endif

                Rect croi = cv::getBoundingBox2D(roiCorners);
                //const int B = 5;
                //cv::rectAppend(croi, B, B, B, B);
                croi = cv::rectOverlapped(croi, Rect(0, 0, img.cols, img.rows));
                circles[i].croi = croi;
            }

            int bT = 0;
            {
                std::vector<cv::Rect> rois;
                for (auto& c : circles)
                    rois.push_back(c.croi);
                rois.push_back(cv::getBoundingBox2D(corners[mi]));
                bT = _estimateThreshold(img, rois);
            }


            int nMiss = 0;

            for (int i = 0; i < _templ.circles.size(); i++)
            {
                auto& c = _templ.circles[i];

                Rect croi = circles[i].croi;
                croi = cv::rectOverlapped(croi, Rect(0, 0, img.cols, img.rows));
                if (croi.empty())
                    continue;

                Mat1b cmask;
                int k = 0;
                for (; k < 5; ++k)
                {
                    Mat1b cimg = img(croi);
                    cv::threshold(cimg, cmask, bT, 255, cv::THRESH_BINARY_INV);

                    double dx = 0, dy = 0;
                    int n = 0;
                    for_each_1c(DWHN1(cmask), [&dx, &dy, &n](uchar v, int x, int y) {
                        if (v > 0)
                        {
                            dx += x;
                            dy += y;
                            n++;
                        }});
                    if (n > 0)
                    {
                        dx = dx/n-croi.width/2;
                        dy = dy/n-croi.height/2;

                        int dxi=int(std::round(dx)), dyi=int(std::round(dy));

                        if (dxi==0 && dyi==0)
                            break;

                        croi.x += dxi;
                        croi.y += dyi;

                        croi = cv::rectOverlapped(croi, Rect(0, 0, img.cols, img.rows));
                        if (croi.empty())
                            break;
                    }
                    else
                        break;
                }

                if (croi.x <= 0 || croi.y <= 0 || croi.x + croi.width >= img.cols-1 || croi.y + croi.height >= img.rows-1)
                    continue;

#if _ARUCOPP_ENABLE_UI
                cv::rectangle(dimg, croi, Scalar(0, 0, 255), 2, CV_AA);
#endif

                int B = (croi.width + croi.height) / 4;
                B = __max(B, 5);
                Rect broi = croi;
                cv::rectAppend(broi, B, B, B, B);
                broi=cv::rectOverlapped(broi, Rect(0, 0, img.cols, img.rows));
                //printf("B=%d\n", B);

                Mat1b cimg = img(croi), bimg=img(broi);

                Mat1b bmask;
                cv::threshold(bimg, bmask, bT, 255, cv::THRESH_BINARY_INV);
#if _ARUCOPP_ENABLE_UI
                bmask.copyTo(dmask(broi));
#endif
                //printf("bT=%d\n", bT);

                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(bmask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

                if (contours.empty())
                    continue;

                //if(contours.size() > 1)
                {
                    auto isOnBorder = [](const std::vector<cv::Point>& c, Size imsize) {
                        for (auto& p : c)
                        {
                            if (p.x <= 0 || p.x >= imsize.width - 1 || p.y <= 0 || p.y >= imsize.height - 1)
                                return true;
                        }
                        return false;
                    };

                    double mscore = 0.f;
                    int mi = -1;
                    for (int i = 0; i < contours.size(); i++)
                    {
                        Rect xroi = cv::getBoundingBox2D(contours[i]);
                        xroi.x+=broi.x;
                        xroi.y+=broi.y;

                        //calc iou with croi
                        Rect x=cv::rectOverlapped(xroi, croi);
                        double iou=double(x.area()) / (xroi.area() + croi.area() - x.area());
                        if (iou > mscore && !isOnBorder(contours[i], broi.size()))
                        {
                            mscore = iou;
                            mi = i;
                        }
                    }
                    if (mscore < 0.2)
                    {
                        printf("mscore=%f\n", mscore);
                        nMiss++;
                        continue;
                    }

                    if (mi > 0)
                        contours[mi].swap(contours[0]);
                }

                if (contours.front().size() < 8)
                    continue;

                auto r = cv::fitEllipse(contours.front());
                r.center += Point2f(broi.x, broi.y);
#if _ARUCOPP_ENABLE_UI
                cv::ellipse(dimg, r, Scalar(0, 255, 0), 1, CV_AA);
#endif
                modelPoints.push_back(Point3f(c.center.x, c.center.y, 0));
                imagePoints.push_back(Point2f(r.center.x, r.center.y));
            }
#if _ARUCOPP_ENABLE_UI
            imshowx("dimg", dimg);
            imshowx("dmask", dmask);
#endif
            _nMiss = nMiss;
            if (nMiss > 0)
            {
                printf("nMiss=%d\n", nMiss);
                //cv::waitKey(0);
            }

            if (modelPoints.size() < 4)
                continue;

            if (cv::solvePnP(modelPoints, imagePoints, K, cv::Mat(), rvec, tvec))
            {
                return true;
            }
        }
        return false;
    }

    int _nMiss = 0;
};