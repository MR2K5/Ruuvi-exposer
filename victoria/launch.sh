#!/bin/bash

./victoria-metrics-prod -retentionPeriod=100y -promscrape.config=/etc/victoria-scrape-config.yml -inmemoryDataFlushInterval=5m

