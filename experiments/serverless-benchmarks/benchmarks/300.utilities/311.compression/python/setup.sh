pip install -r requirements.txt --target .

wget https://github.com/spcl/serverless-benchmarks-data/archive/refs/tags/v1.0.zip
unzip v1.0.zip
cp -r serverless-benchmarks-data-1.0/300.utilities/311.compression/* .
rm -rf v1.0.zip
rm -rf serverless-benchmarks-data-1.0

touch acmart-master.zip
